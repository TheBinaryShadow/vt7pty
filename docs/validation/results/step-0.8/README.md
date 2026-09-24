# Step 0.8 Raw Windows 7 Results

These are the unmodified JSON, text, and diagnostic records supplied for the
[Step 0.8 completion](../../2026-09-24-step-0.8-completion.md). The JSON files
retain the complete installed-update inventories and per-case output. The
temporary agent-rejection fixture directories were not retained because their
results are captured in the JSON records.

| File | SHA-256 |
| --- | --- |
| `windows7-esu-20260924-161255.json` | `52e6397980144c0eeb23ec6841755b11f99c6917ac26e0da645a63019dcfb2cd` |
| `windows7-esu-20260924-161255.txt` | `b3e226a70dbb1561031fca6a866859db6ecf49419a5a1f274a20a94bd8cdc2a9` |
| `windows7-esu-20260924-161255-diagnostics.jsonl` | `72263ae71cb92321b0996b1ea40fad2687a30663d370545b2e84188202ac5c7b` |
| `windows7-esu-20260924-161255-diagnostic-bundle/diagnostic-manifest.json` | `d1f5b300dd457f792ef0fbeffa5d79ad511bc4730d43c65997c0264af99dbf5a` |
| `windows7-esu-20260924-161255-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-esu-20260924-161255-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `72263ae71cb92321b0996b1ea40fad2687a30663d370545b2e84188202ac5c7b` |
| `windows7-nonesu-20260924-161233.json` | `847a1e09d9c431c45572041785d6c18f260ecfd08145c5b291097cb51fd374d5` |
| `windows7-nonesu-20260924-161233.txt` | `29b112e7b690fd6a4d4c448423df4b3b280e7729a08f2d181d019eda6742d051` |
| `windows7-nonesu-20260924-161233-diagnostics.jsonl` | `8d8374680d1400d7c4b7740e33607bb4e378528a0acbd5fb4ddf6664f599c768` |
| `windows7-nonesu-20260924-161233-diagnostic-bundle/diagnostic-manifest.json` | `51057b1163904a6ed6fd8fcd6b16620a0335aaa4e5338f4e765ee913bbb9c477` |
| `windows7-nonesu-20260924-161233-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-nonesu-20260924-161233-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `8d8374680d1400d7c4b7740e33607bb4e378528a0acbd5fb4ddf6664f599c768` |

Each bundled diagnostic log is byte-identical to its standalone copy, and the
corresponding bundle manifest records the same hash.
