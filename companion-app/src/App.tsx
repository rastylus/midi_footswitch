import { useEffect, useMemo, useRef, useState } from 'react'

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

type AppPage = 'editor' | 'monitor'

type MidiMonitorType =
  | 'Note On'
  | 'Note Off'
  | 'Control Change (CC)'
  | 'Program Change'
  | 'Pitch Bend'
  | 'Aftertouch'
  | 'System'
  | 'Unknown'

type MonitorMessage = {
  id: number
  timestamp: string
  device: string
  type: MidiMonitorType
  rawHex: string
  detail: string
  channel: number | null
}

const MONITOR_TYPES: MidiMonitorType[] = [
  'Note On',
  'Note Off',
  'Control Change (CC)',
  'Program Change',
  'Pitch Bend',
  'Aftertouch',
  'System',
  'Unknown',
]

const MONITOR_CHANNELS = Array.from({ length: 16 }, (_, i) => i + 1)

const STORAGE_KEY = 'footswitch-companion-v1'
const MIDI_OUTPUT_ID_KEY = 'footswitch-midi-output-id'
const MIDI_CAPTURE_IDS_KEY = 'footswitch-midi-capture-ids'

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

function noteName(note: number): string {
  const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
  const octave = Math.floor(note / 12) - 1
  return `${names[note % 12]}${octave}`
}

function parseMidiMessage(data: Uint8Array): {
  type: MidiMonitorType
  detail: string
  channel: number | null
} {
  const status = data[0] ?? 0

  if (
    status === 0xf0 &&
    data.length >= 9 &&
    data[1] === 0x7d &&
    data[2] === 0x46 &&
    data[3] === 0x53 &&
    data[4] === 0x57 &&
    data[5] === 0x13
  ) {
    return {
      type: 'System',
      detail: `Pico firmware v${data[6]}.${data[7]}.${data[8]}`,
      channel: null,
    }
  }

  const high = status & 0xf0
  const channel = (status & 0x0f) + 1
  const d1 = data[1] ?? 0
  const d2 = data[2] ?? 0

  if (high === 0x90) {
    if (d2 === 0) {
      return {
        type: 'Note Off',
        detail: `Ch ${channel} - Note: ${d1} (${noteName(d1)}) Vel: 0`,
        channel,
      }
    }
    return {
      type: 'Note On',
      detail: `Ch ${channel} - Note: ${d1} (${noteName(d1)}) Vel: ${d2} ON`,
      channel,
    }
  }

  if (high === 0x80) {
    return {
      type: 'Note Off',
      detail: `Ch ${channel} - Note: ${d1} (${noteName(d1)}) Vel: ${d2}`,
      channel,
    }
  }

  if (high === 0xb0) {
    return {
      type: 'Control Change (CC)',
      detail: `Ch ${channel} - CC: ${d1} Value: ${d2}`,
      channel,
    }
  }

  if (high === 0xc0) {
    return {
      type: 'Program Change',
      detail: `Ch ${channel} - Program: ${d1}`,
      channel,
    }
  }

  if (high === 0xe0) {
    const bend = ((d2 << 7) | d1) - 8192
    return {
      type: 'Pitch Bend',
      detail: `Ch ${channel} - Bend: ${bend}`,
      channel,
    }
  }

  if (high === 0xd0 || high === 0xa0) {
    return {
      type: 'Aftertouch',
      detail: `Ch ${channel} - Data: ${d1} ${d2}`,
      channel,
    }
  }

  if (status >= 0xf0) {
    return {
      type: 'System',
      detail: `System message: 0x${status.toString(16).toUpperCase()}`,
      channel: null,
    }
  }

  return {
    type: 'Unknown',
    detail: 'Unrecognized MIDI message',
    channel: null,
  }
}

function pickPreferredOutputId(outputs: MIDIOutput[]): string {
  if (outputs.length === 0) return ''

  const preferred = outputs.find((out) => {
    const name = (out.name ?? '').toLowerCase()
    return (
      name.includes('pico') ||
      name.includes('kb2040') ||
      name.includes('footswitch')
    )
  })

  return preferred?.id ?? outputs[0].id
}

function isLikelyFootswitchOutputName(name: string): boolean {
  const n = name.toLowerCase()
  return n.includes('pico') || n.includes('kb2040') || n.includes('footswitch')
}

function App() {
  const [page, setPage] = useState<AppPage>('editor')
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
  const [midiInputs, setMidiInputs] = useState<MIDIInput[]>([])
  const [selectedOutputId, setSelectedOutputId] = useState('')
  const [transferStatus, setTransferStatus] = useState('Not connected')
  const [monitorRunning, setMonitorRunning] = useState(true)
  const [deduplicateEnabled, setDeduplicateEnabled] = useState(true)
  const [messages, setMessages] = useState<MonitorMessage[]>([])
  const [selectedCaptureInputIds, setSelectedCaptureInputIds] = useState<string[]>([])
  const [selectedDeviceFilters, setSelectedDeviceFilters] = useState<string[]>([])
  const [selectedTypeFilters, setSelectedTypeFilters] = useState<MidiMonitorType[]>(MONITOR_TYPES)
  const [selectedChannelFilters, setSelectedChannelFilters] = useState<number[]>(MONITOR_CHANNELS)
  const messageIdRef = useRef(1)
  const monitorRunningRef = useRef(monitorRunning)
  const selectedCaptureInputIdsRef = useRef<string[]>([])
  const deduplicateEnabledRef = useRef(deduplicateEnabled)
  const lastEventSeenRef = useRef<Map<string, number>>(new Map())

  monitorRunningRef.current = monitorRunning
  selectedCaptureInputIdsRef.current = selectedCaptureInputIds
  deduplicateEnabledRef.current = deduplicateEnabled

  useEffect(() => {
    localStorage.setItem(MIDI_OUTPUT_ID_KEY, selectedOutputId)
  }, [selectedOutputId])

  useEffect(() => {
    localStorage.setItem(MIDI_CAPTURE_IDS_KEY, JSON.stringify(selectedCaptureInputIds))
  }, [selectedCaptureInputIds])

  const activeBank = useMemo(
    () => config.banks.find((b) => b.bank === selectedBank) ?? config.banks[0],
    [config.banks, selectedBank],
  )

  const visibleMessages = useMemo(() => {
    return messages.filter((msg) => {
      if (!selectedDeviceFilters.includes(msg.device)) return false
      if (!selectedTypeFilters.includes(msg.type)) return false

      // Channel filtering applies to channelized MIDI messages only.
      if (msg.channel !== null && !selectedChannelFilters.includes(msg.channel)) return false

      return true
    })
  }, [messages, selectedDeviceFilters, selectedTypeFilters, selectedChannelFilters])

  const activeDevices = useMemo(() => {
    const devices = new Set(messages.map((m) => m.device))
    return devices.size
  }, [messages])

  const filteredTypeCount = useMemo(() => {
    const types = new Set(visibleMessages.map((m) => m.type))
    return types.size
  }, [visibleMessages])

  const filteredChannelCount = useMemo(() => {
    const channels = new Set(visibleMessages.filter((m) => m.channel !== null).map((m) => m.channel))
    return channels.size
  }, [visibleMessages])

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
      const inputs = Array.from(access.inputs.values())
      const savedOutputId = localStorage.getItem(MIDI_OUTPUT_ID_KEY) ?? ''
      const savedCaptureIdsRaw = localStorage.getItem(MIDI_CAPTURE_IDS_KEY)
      let savedCaptureIds: string[] = []
      if (savedCaptureIdsRaw) {
        try {
          const parsed = JSON.parse(savedCaptureIdsRaw) as string[]
          if (Array.isArray(parsed)) {
            savedCaptureIds = parsed
          }
        } catch {
          savedCaptureIds = []
        }
      }

      setMidiAccess(access)
      setMidiOutputs(outputs)
      setMidiInputs(inputs)

      const chosenOutputId =
        (savedOutputId && outputs.some((o) => o.id === savedOutputId))
          ? savedOutputId
          : pickPreferredOutputId(outputs)
      setSelectedOutputId(chosenOutputId)

      const validSavedCaptureIds = savedCaptureIds.filter((id) => inputs.some((i) => i.id === id))
      if (validSavedCaptureIds.length > 0) {
        setSelectedCaptureInputIds(validSavedCaptureIds)
      } else {
        setSelectedCaptureInputIds(inputs.map((i) => i.id))
      }

      const deviceNames = inputs.map((i) => i.name ?? i.id)
      setSelectedDeviceFilters(deviceNames)

      const bindInputs = () => {
        access.inputs.forEach((input) => {
          input.onmidimessage = (event) => {
            if (!monitorRunningRef.current) return
            if (
              selectedCaptureInputIdsRef.current.length > 0 &&
              !selectedCaptureInputIdsRef.current.includes(input.id)
            ) {
              return
            }

            if (!event.data) {
              return
            }

            const bytes = Array.from(event.data)
            const rawHex = bytes
              .map((b) => b.toString(16).toUpperCase().padStart(2, '0'))
              .join(' ')
            const parsed = parseMidiMessage(event.data)

            const deviceName = input.name ?? input.id

            const dedupeKey = `${deviceName}|${rawHex}|${parsed.type}|${parsed.channel ?? 'NA'}`
            const nowMs = Date.now()
            if (deduplicateEnabledRef.current) {
              const lastMs = lastEventSeenRef.current.get(dedupeKey)
              if (typeof lastMs === 'number' && nowMs - lastMs <= 8) {
                return
              }
            }
            lastEventSeenRef.current.set(dedupeKey, nowMs)

            const row: MonitorMessage = {
              id: messageIdRef.current++,
              timestamp: new Date().toLocaleTimeString('en-GB', {
                hour12: false,
                hour: '2-digit',
                minute: '2-digit',
                second: '2-digit',
                fractionalSecondDigits: 3,
              }),
              device: deviceName,
              type: parsed.type,
              rawHex,
              detail: parsed.detail,
              channel: parsed.channel,
            }

            setMessages((current) => [row, ...current].slice(0, 500))
          }
        })
      }

      bindInputs()
      access.onstatechange = () => {
        const nextOutputs = Array.from(access.outputs.values())
        const nextInputs = Array.from(access.inputs.values())

        setMidiOutputs(nextOutputs)
        setMidiInputs(nextInputs)

        setSelectedOutputId((current) => {
          if (current && nextOutputs.some((o) => o.id === current)) return current
          return nextOutputs[0]?.id ?? ''
        })

        setSelectedCaptureInputIds((current) => {
          const nextIds = nextInputs.map((i) => i.id)
          const kept = current.filter((id) => nextIds.includes(id))
          if (kept.length > 0) return kept
          return nextIds
        })

        setSelectedDeviceFilters((current) => {
          const names = nextInputs.map((i) => i.name ?? i.id)
          const kept = current.filter((name) => names.includes(name))
          if (kept.length > 0) return kept
          return names
        })

        bindInputs()
      }

      if (outputs.length > 0) {
        setSelectedOutputId((current) => {
          if (current && outputs.some((o) => o.id === current)) return current
          return pickPreferredOutputId(outputs)
        })
        setTransferStatus(`Connected: ${outputs.length} output(s), ${inputs.length} input(s)`)
      } else {
        setTransferStatus(`Connected: 0 output(s), ${inputs.length} input(s)`)
      }
    } catch {
      setTransferStatus('MIDI connection failed (allow SysEx in browser prompt).')
    }
  }

  useEffect(() => {
    if (!('requestMIDIAccess' in navigator)) {
      return
    }

    // Try reconnect on app load. If permissions are unavailable, user can still use Connect MIDI.
    void connectMidi()
  }, [])

  const sendSysex = (output: MIDIOutput, payload: number[]) => {
    output.send([0xf0, 0x7d, 0x46, 0x53, 0x57, ...payload, 0xf7])
  }

  const checkPicoFirmware = () => {
    if (!midiAccess) {
      setTransferStatus('Connect MIDI first.')
      return
    }

    const output = midiAccess.outputs.get(selectedOutputId)
    if (!output) {
      setTransferStatus('Select a valid MIDI output device.')
      return
    }

    sendSysex(output, [0x13])
    setTransferStatus(`Sent firmware probe to ${output.name ?? 'MIDI device'}`)
  }

  const deviceFilterOptions = useMemo(
    () => Array.from(new Set(midiInputs.map((input) => input.name ?? input.id))),
    [midiInputs],
  )

  const toggleStringSelection = (
    value: string,
    selected: string[],
    setSelected: (next: string[]) => void,
  ) => {
    if (selected.includes(value)) {
      setSelected(selected.filter((v) => v !== value))
    } else {
      setSelected([...selected, value])
    }
  }

  const toggleTypeSelection = (value: MidiMonitorType) => {
    if (selectedTypeFilters.includes(value)) {
      setSelectedTypeFilters(selectedTypeFilters.filter((v) => v !== value))
    } else {
      setSelectedTypeFilters([...selectedTypeFilters, value])
    }
  }

  const toggleChannelSelection = (value: number) => {
    if (selectedChannelFilters.includes(value)) {
      setSelectedChannelFilters(selectedChannelFilters.filter((v) => v !== value))
    } else {
      setSelectedChannelFilters([...selectedChannelFilters, value])
    }
  }

  const sendToFootswitch = () => {
    if (!midiAccess) {
      setTransferStatus('Connect MIDI first.')
      return
    }

    const allOutputs = Array.from(midiAccess.outputs.values())
    if (allOutputs.length === 0) {
      setTransferStatus('No MIDI outputs available.')
      return
    }

    const selectedOutput = midiAccess.outputs.get(selectedOutputId)
    const preferredOutputId = pickPreferredOutputId(allOutputs)
    const preferredOutput = midiAccess.outputs.get(preferredOutputId)

    // If a non-footswitch output is currently selected and a likely footswitch output exists,
    // route transfer to the footswitch output automatically.
    let output = selectedOutput ?? preferredOutput
    if (
      selectedOutput &&
      preferredOutput &&
      selectedOutput.id !== preferredOutput.id &&
      !isLikelyFootswitchOutputName(selectedOutput.name ?? '') &&
      isLikelyFootswitchOutputName(preferredOutput.name ?? '')
    ) {
      output = preferredOutput
      setSelectedOutputId(preferredOutput.id)
    }

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
          <button onClick={connectMidi}>
            {midiAccess ? 'Refresh MIDI' : 'Connect MIDI'}
          </button>
          <button onClick={sendToFootswitch}>Send to Footswitch</button>
          <button onClick={checkPicoFirmware}>Check Pico Firmware</button>
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

      <section className="tabs">
        <button className={page === 'editor' ? 'tab active' : 'tab'} onClick={() => setPage('editor')}>
          Mapping Editor
        </button>
        <button className={page === 'monitor' ? 'tab active' : 'tab'} onClick={() => setPage('monitor')}>
          MIDI Monitor
        </button>
      </section>

      {page === 'editor' ? (
        <>
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
        </>
      ) : (
        <>
          <section className="monitorStats">
            <div className="statCard">
              <strong>{monitorRunning ? 'RUNNING' : 'PAUSED'}</strong>
              <span>{visibleMessages.length} Messages</span>
            </div>
            <div className="statCard">
              <strong>{activeDevices}</strong>
              <span>Devices</span>
            </div>
            <div className="statCard">
              <strong>{filteredTypeCount}</strong>
              <span>Message Types</span>
            </div>
            <div className="statCard">
              <strong>{filteredChannelCount}</strong>
              <span>MIDI Channels</span>
            </div>
          </section>

          <section className="toolbar">
            <button onClick={() => setMonitorRunning((v) => !v)}>
              {monitorRunning ? 'Pause Monitor' : 'Resume Monitor'}
            </button>
            <button onClick={() => setMessages([])}>Clear Log</button>
            <label>
              <input
                type="checkbox"
                checked={deduplicateEnabled}
                onChange={(e) => setDeduplicateEnabled(e.target.checked)}
              />
              Deduplicate mirrored events
            </label>
          </section>

          <section className="monitorLayout">
            <aside className="monitorSidebar">
              <div className="filterGroup">
                <div className="filterHeader">
                  <h3>Capture Inputs</h3>
                  <div className="filterButtons">
                    <button onClick={() => setSelectedCaptureInputIds(midiInputs.map((i) => i.id))}>All</button>
                    <button onClick={() => setSelectedCaptureInputIds([])}>None</button>
                  </div>
                </div>
                <div className="checkList">
                  {midiInputs.map((input) => (
                    <label key={input.id} className="checkItem">
                      <input
                        type="checkbox"
                        checked={selectedCaptureInputIds.includes(input.id)}
                        onChange={() =>
                          toggleStringSelection(
                            input.id,
                            selectedCaptureInputIds,
                            setSelectedCaptureInputIds,
                          )
                        }
                      />
                      <span>{input.name ?? input.id}</span>
                    </label>
                  ))}
                </div>
              </div>

              <div className="filterGroup">
                <div className="filterHeader">
                  <h3>Device Filter</h3>
                  <div className="filterButtons">
                    <button onClick={() => setSelectedDeviceFilters(deviceFilterOptions)}>All</button>
                    <button onClick={() => setSelectedDeviceFilters([])}>None</button>
                  </div>
                </div>
                <div className="checkList">
                  {deviceFilterOptions.map((name) => (
                    <label key={name} className="checkItem">
                      <input
                        type="checkbox"
                        checked={selectedDeviceFilters.includes(name)}
                        onChange={() =>
                          toggleStringSelection(name, selectedDeviceFilters, setSelectedDeviceFilters)
                        }
                      />
                      <span>{name}</span>
                    </label>
                  ))}
                </div>
              </div>

              <div className="filterGroup">
                <div className="filterHeader">
                  <h3>Message Types</h3>
                  <div className="filterButtons">
                    <button onClick={() => setSelectedTypeFilters(MONITOR_TYPES)}>All</button>
                    <button onClick={() => setSelectedTypeFilters([])}>None</button>
                  </div>
                </div>
                <div className="checkList">
                  {MONITOR_TYPES.map((type) => (
                    <label key={type} className="checkItem">
                      <input
                        type="checkbox"
                        checked={selectedTypeFilters.includes(type)}
                        onChange={() => toggleTypeSelection(type)}
                      />
                      <span>{type}</span>
                    </label>
                  ))}
                </div>
              </div>

              <div className="filterGroup">
                <div className="filterHeader">
                  <h3>MIDI Channels</h3>
                  <div className="filterButtons">
                    <button onClick={() => setSelectedChannelFilters(MONITOR_CHANNELS)}>All</button>
                    <button onClick={() => setSelectedChannelFilters([])}>None</button>
                  </div>
                </div>
                <div className="checkList checkListCompact">
                  {MONITOR_CHANNELS.map((ch) => (
                    <label key={ch} className="checkItem">
                      <input
                        type="checkbox"
                        checked={selectedChannelFilters.includes(ch)}
                        onChange={() => toggleChannelSelection(ch)}
                      />
                      <span>Ch {ch}</span>
                    </label>
                  ))}
                </div>
              </div>
            </aside>

            <section className="monitorTableWrap">
              <table className="monitorTable">
                <thead>
                  <tr>
                    <th>Timestamp</th>
                    <th>Device</th>
                    <th>Type</th>
                    <th>Data</th>
                    <th>Channel</th>
                  </tr>
                </thead>
                <tbody>
                  {visibleMessages.map((msg) => (
                    <tr key={msg.id}>
                      <td>{msg.timestamp}</td>
                      <td>{msg.device}</td>
                      <td>{msg.type}</td>
                      <td>
                        <span className="hex">{msg.rawHex}</span>
                        <span>{msg.detail}</span>
                      </td>
                      <td>{msg.channel ?? '-'}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </section>
          </section>
        </>
      )}
    </main>
  )
}

export default App
