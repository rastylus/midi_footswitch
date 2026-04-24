import { useMemo, useState } from 'react'

type MidiType = 'NOTE' | 'CC' | 'PC' | 'TRANSPORT'
type Behavior = 'MOMENTARY' | 'TOGGLE'

type SwitchMapping = {
  index: number
  midiType: MidiType
  behavior: Behavior
  number: number
  channel: number
  onValue: number
  offValue: number
}

type BankLayout = {
  bank: number
  switches: SwitchMapping[]
}

type AppConfig = {
  globalMidiChannel: number
  pixelBrightness: number
  banks: BankLayout[]
}

const STORAGE_KEY = 'footswitch-companion-v1'

function buildDefaults(): AppConfig {
  const banks: BankLayout[] = []
  for (let bank = 1; bank <= 8; bank++) {
    const switches: SwitchMapping[] = []
    for (let i = 0; i < 8; i++) {
      switches.push({
        index: i + 1,
        midiType: 'NOTE',
        behavior: 'MOMENTARY',
        number: 60 + i,
        channel: 1,
        onValue: 127,
        offValue: 0,
      })
    }
    banks.push({ bank, switches })
  }

  for (let i = 0; i < 8; i++) {
    banks[0].switches[i].midiType = 'CC'
    banks[0].switches[i].behavior = 'TOGGLE'
    banks[0].switches[i].number = 20 + i
  }

  for (let bank = 2; bank <= 7; bank++) {
    const offset = (bank - 2) * 8
    for (let i = 0; i < 8; i++) {
      banks[bank - 1].switches[i].midiType = 'NOTE'
      banks[bank - 1].switches[i].behavior = 'MOMENTARY'
      banks[bank - 1].switches[i].number = (60 + i + offset) % 128
    }
  }

  const transportLayout = [0, 1, 2, 3, 3, 2, 1, 0]
  for (let i = 0; i < 8; i++) {
    banks[7].switches[i].midiType = 'TRANSPORT'
    banks[7].switches[i].behavior = 'MOMENTARY'
    banks[7].switches[i].number = transportLayout[i]
  }

  return { globalMidiChannel: 1, pixelBrightness: 5, banks }
}

function midiTypeToByte(type: MidiType): number {
  switch (type) {
    case 'NOTE':
      return 0
    case 'CC':
      return 1
    case 'PC':
      return 2
    case 'TRANSPORT':
      return 3
    default:
      return 0
  }
}

function behaviorToByte(behavior: Behavior): number {
  return behavior === 'TOGGLE' ? 1 : 0
}

function App() {
  const [config, setConfig] = useState<AppConfig>(() => {
    const saved = localStorage.getItem(STORAGE_KEY)
    if (saved) {
      try {
        return JSON.parse(saved) as AppConfig
      } catch {
        return buildDefaults()
      }
    }
    return buildDefaults()
  })
  const [selectedBank, setSelectedBank] = useState(1)
  const [midiAccess, setMidiAccess] = useState<MIDIAccess | null>(null)
  const [midiOutputs, setMidiOutputs] = useState<MIDIOutput[]>([])
  const [selectedOutputId, setSelectedOutputId] = useState('')
  const [transferStatus, setTransferStatus] = useState('Not connected')

  const activeBank = useMemo(
    () => config.banks.find((b) => b.bank === selectedBank) ?? config.banks[0],
    [config.banks, selectedBank],
  )

  const updateSwitch = (index: number, patch: Partial<SwitchMapping>) => {
    setConfig((current) => {
      const next = {
        ...current,
        banks: current.banks.map((bank) =>
          bank.bank !== selectedBank
            ? bank
            : {
                ...bank,
                switches: bank.switches.map((sw) =>
                  sw.index === index ? { ...sw, ...patch } : sw,
                ),
              },
        ),
      }
      localStorage.setItem(STORAGE_KEY, JSON.stringify(next))
      return next
    })
  }

  const exportConfig = () => {
    const blob = new Blob([JSON.stringify(config, null, 2)], {
      type: 'application/json',
    })
    const url = URL.createObjectURL(blob)
    const link = document.createElement('a')
    link.href = url
    link.download = 'footswitch-mapping.json'
    link.click()
    URL.revokeObjectURL(url)
  }

  const importConfig = (file: File | null) => {
    if (!file) return
    const reader = new FileReader()
    reader.onload = () => {
      try {
        const incoming = JSON.parse(String(reader.result)) as AppConfig
        setConfig(incoming)
        localStorage.setItem(STORAGE_KEY, JSON.stringify(incoming))
      } catch {
        window.alert('Invalid JSON file.')
      }
    }
    reader.readAsText(file)
  }

  const connectMidi = async () => {
    if (!('requestMIDIAccess' in navigator)) {
      setTransferStatus('WebMIDI is not available in this browser.')
      return
    }

    try {
      const access = await navigator.requestMIDIAccess({ sysex: true })
      const outputs = Array.from(access.outputs.values())
      setMidiAccess(access)
      setMidiOutputs(outputs)

      if (outputs.length > 0) {
        setSelectedOutputId(outputs[0].id)
        setTransferStatus(`Connected: ${outputs.length} MIDI output(s) found`)
      } else {
        setTransferStatus('No MIDI outputs found')
      }
    } catch {
      setTransferStatus('MIDI connection failed (allow SysEx in browser prompt).')
    }
  }

  const sendSysex = (output: MIDIOutput, payload: number[]) => {
    output.send([0xf0, 0x7d, 0x46, 0x53, 0x57, ...payload, 0xf7])
  }

  const sendToFootswitch = () => {
    if (!midiAccess) {
      setTransferStatus('Connect MIDI first.')
      return
    }

    const output = midiAccess.outputs.get(selectedOutputId)
    if (!output) {
      setTransferStatus('Select a valid MIDI output device.')
      return
    }

    // Global settings
    sendSysex(output, [0x10, config.globalMidiChannel, config.pixelBrightness])

    // Per-switch mappings
    for (const bank of config.banks) {
      for (const sw of bank.switches) {
        sendSysex(output, [
          0x11,
          bank.bank,
          sw.index,
          midiTypeToByte(sw.midiType),
          behaviorToByte(sw.behavior),
          sw.number,
          sw.channel,
          sw.onValue,
          sw.offValue,
        ])
      }
    }

    // Commit/save
    sendSysex(output, [0x12])
    setTransferStatus(`Sent mapping to ${output.name ?? 'MIDI device'}`)
  }

  return (
    <main className="shell">
      <header className="topbar">
        <div>
          <h1>MIDI Footswitch Companion</h1>
          <p>Configure banks, switch mappings, and export presets.</p>
        </div>
        <div className="actions">
          <button onClick={() => setConfig(buildDefaults())}>Factory Defaults</button>
          <button onClick={exportConfig}>Export JSON</button>
          <button onClick={connectMidi}>Connect MIDI</button>
          <button onClick={sendToFootswitch}>Send to Footswitch</button>
          <label className="filePick">
            Import JSON
            <input
              type="file"
              accept="application/json"
              onChange={(e) => importConfig(e.target.files?.[0] ?? null)}
            />
          </label>
        </div>
      </header>

      <section className="toolbar">
        <label>
          Global MIDI Channel
          <input
            type="number"
            min={1}
            max={16}
            value={config.globalMidiChannel}
            onChange={(e) => {
              const value = Math.max(1, Math.min(16, Number(e.target.value) || 1))
              const next = { ...config, globalMidiChannel: value }
              setConfig(next)
              localStorage.setItem(STORAGE_KEY, JSON.stringify(next))
            }}
          />
        </label>

        <label>
          Bank
          <select
            value={selectedBank}
            onChange={(e) => setSelectedBank(Number(e.target.value))}
          >
            {config.banks.map((b) => (
              <option key={b.bank} value={b.bank}>
                Bank {b.bank}
              </option>
            ))}
          </select>
        </label>

        <label>
          Brightness
          <input
            type="number"
            min={0}
            max={10}
            value={config.pixelBrightness}
            onChange={(e) => {
              const value = Math.max(0, Math.min(10, Number(e.target.value) || 0))
              const next = { ...config, pixelBrightness: value }
              setConfig(next)
              localStorage.setItem(STORAGE_KEY, JSON.stringify(next))
            }}
          />
        </label>

        <label>
          MIDI Output
          <select
            value={selectedOutputId}
            onChange={(e) => setSelectedOutputId(e.target.value)}
          >
            <option value="">Select output...</option>
            {midiOutputs.map((out) => (
              <option key={out.id} value={out.id}>
                {out.name ?? out.id}
              </option>
            ))}
          </select>
        </label>

        <label>
          Transfer Status
          <input type="text" value={transferStatus} readOnly />
        </label>
      </section>

      <section className="grid">
        {activeBank.switches.map((sw) => (
          <article className="card" key={sw.index}>
            <h3>SW {sw.index}</h3>
            <label>
              Type
              <select
                value={sw.midiType}
                onChange={(e) => updateSwitch(sw.index, { midiType: e.target.value as MidiType })}
              >
                <option>NOTE</option>
                <option>CC</option>
                <option>PC</option>
                <option>TRANSPORT</option>
              </select>
            </label>
            <label>
              Behavior
              <select
                value={sw.behavior}
                onChange={(e) =>
                  updateSwitch(sw.index, { behavior: e.target.value as Behavior })
                }
              >
                <option>MOMENTARY</option>
                <option>TOGGLE</option>
              </select>
            </label>
            <label>
              Number
              <input
                type="number"
                min={0}
                max={127}
                value={sw.number}
                onChange={(e) => updateSwitch(sw.index, { number: Number(e.target.value) || 0 })}
              />
            </label>
            <label>
              Channel
              <input
                type="number"
                min={1}
                max={16}
                value={sw.channel}
                onChange={(e) => updateSwitch(sw.index, { channel: Number(e.target.value) || 1 })}
              />
            </label>
            <label>
              On Value
              <input
                type="number"
                min={0}
                max={127}
                value={sw.onValue}
                onChange={(e) => updateSwitch(sw.index, { onValue: Number(e.target.value) || 0 })}
              />
            </label>
            <label>
              Off Value
              <input
                type="number"
                min={0}
                max={127}
                value={sw.offValue}
                onChange={(e) => updateSwitch(sw.index, { offValue: Number(e.target.value) || 0 })}
              />
            </label>
          </article>
        ))}
      </section>
    </main>
  )
}

export default App
