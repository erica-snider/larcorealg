These are answers to the plan's open questions.

| Open question | Answer  | Notes
|---|---|---|
| Output pattern | bare `%v`, no timestamp | Want to simplify output to ease comparisons. Time context is available elsewhere |
| Header home | larcorealg/larcorealg/LoggingUtil/Logging.h | Separate type of functionality than in CoreUtils, so a new sub-directory |
| Hot-path guard style | Context dependendent. Preferred default is to leave as is unless expected message output is extremely high (e.g., for raw data loops, decompression loops). In those cases, a function-local one-count guard with summary on exit is acceptable. For all other cases, propose leaving it as is, accumulate occurrences and ask...If expected message frequency is not obvious, Where frequency cannot be easily deduced, then identify cases, ask for input. See decisions below for currently identified cases. | There is not a rule to govern all cases, so just collect them all and ask if the frequency is not obvious. Most will be simple. |
| File destinations from old config | keep as a second sink | Want to retain possibly useful functionality from the old version | 
| Deprecation window for renamed public init functions | break immediately | Best to just work through the pain now |

Notes on the hot-path guard style for the current set of files in larcorealg: 
- lardataobj/lardataobj/RecoBase/Event.cxx :  leave this as is
- lardataobj/lardataobj/RawData/raw.cxx :  function local one-count guard, so once per vector input / waveform / wire
- larcorealg/larcorealg/TestUtils/unit_test_base.h  :  leave this as is
- GeometryBadIntPoint  called in `GeometryCore.cxx`` :   leave as is. Should occur infrequently
- No end-of-job spdlog statistics need to be accumulated
- LogVerbatim in tests:  log as level 'info' in spdlog
