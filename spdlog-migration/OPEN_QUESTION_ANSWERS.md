These are the plan's open questions, restated as parameters the workflow needs. Each
must have a committed default, or the workflow will stop and ask.

| Open question | Answer  | Notes
|---|---|---|
| Output pattern | bare `%v`, no timestamp | Want to simplify output to ease comparisons. Time context is available elsewhere |
| Header home | larcorealg/larcorealg/LoggingUtil/Logging.h | Separate type of functionality than in CoreUtils |
| Hot-path guard style | Context dependendent. Identify cases, ask for input. | There is not a rule to govern all cases, so just ask. Most will be simple. |
| File destinations from old config | keep as a second sink | Want to replicate possibly useful functionality in old version | 
| Deprecation window for renamed public init functions | break immediately | Just do it |

Notes on the hot-path guard style for the current set of files in larcorealg: 
- lardataobj/lardataobj/RecoBase/Event.cxx :  leave this as is
- lardataobj/lardataobj/RawData/raw.cxx :  function local one-count guard, so once per vector input / waveform / wire
- larcorealg/larcorealg/TestUtils/unit_test_base.h  :  leave this as is
- GeometryBadIntPoint  called in `GeometryCore.cxx`` :   leave as is. Should occur infrequently
- No end-of-job spdlog statistics need to be accumulated
- LogVerbatim in tests:  log as level 'info' in spdlog
