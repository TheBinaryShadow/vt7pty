# Step 0.9 Raw Windows 7 Results

These are unmodified JSON, text, and diagnostic records supplied for the
[Step 0.9 completion](../../2026-09-25-step-0.9-completion.md). All 18 retained
files match the returned originals byte for byte. The JSON files retain the
complete installed-update inventories, test outputs, and machine identities.
The temporary agent-rejection fixture directories were not retained because
their outcomes and diagnostic checks are captured in the JSON records.

The generated `release-review.json` is also preserved here. It records a
passing exact-asset review with zero failures and `PublicationApproved: false`;
its SHA-256 is `6a71b6f79a0840ff6235d96a2a201e64da2a1ef63e5e4c4a6d1ef0ae79ab79ea`.

| File | SHA-256 |
| --- | --- |
| `windows7-esu-20260924-234529-diagnostic-bundle/diagnostic-manifest.json` | `08a0a55d23c5d70b4d239bc8de0960eca6623eb21fd3da0b250f092dc846a773` |
| `windows7-esu-20260924-234529-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-esu-20260924-234529-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `b8110044f01e7f4c38a1fa50e32936fcfc3163e3ec4611635216dae79d1e8388` |
| `windows7-esu-20260924-234529-diagnostics.jsonl` | `b8110044f01e7f4c38a1fa50e32936fcfc3163e3ec4611635216dae79d1e8388` |
| `windows7-esu-20260924-234529.json` | `e9d30560e4a92559ef0684cac9545696db55def5957336368051eaa76d23a2a1` |
| `windows7-esu-20260924-234529.txt` | `6aee789874599303940689e032ae2b89d97155b345ff22fde599817f56652f4f` |
| `windows7-legacy-20260924-234451-diagnostic-bundle/diagnostic-manifest.json` | `1618b9f7c2e6b97852b6e8594409ac445088d6d816af193a0067beec0dcc150f` |
| `windows7-legacy-20260924-234451-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-legacy-20260924-234451-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `39c35eea25236266294704afaf6ba3b1980a70585cf9165a0629223acd69b3f3` |
| `windows7-legacy-20260924-234451-diagnostics.jsonl` | `39c35eea25236266294704afaf6ba3b1980a70585cf9165a0629223acd69b3f3` |
| `windows7-legacy-20260924-234451.json` | `46d18ad24324bbb172eb1bf6ec0142ad3324a8187de2d13ef01ad1dadc9d93f4` |
| `windows7-legacy-20260924-234451.txt` | `744b2d33542cc5ac0bdd1521c252b6e91f97499e1edd87b8dcb3d3937e248f22` |
| `windows7-nonesu-20260924-234342-diagnostic-bundle/diagnostic-manifest.json` | `4611b3ce258d892387769132a86f4075d736518893264bc9fa9edf11ab63157e` |
| `windows7-nonesu-20260924-234342-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-nonesu-20260924-234342-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `91c3a15af017e29b735f7f2c2ad93b75899daf6d60faba35f6d661ee83f62344` |
| `windows7-nonesu-20260924-234342-diagnostics.jsonl` | `91c3a15af017e29b735f7f2c2ad93b75899daf6d60faba35f6d661ee83f62344` |
| `windows7-nonesu-20260924-234342.json` | `4b40d048b9763753c986afad221974ca52c7cd53883a855c9e2f88520d5ba28e` |
| `windows7-nonesu-20260924-234342.txt` | `ac01e3ac80083f60dd0a47f96523410482c8ebe3f462ca477c98c65cecc2dcc6` |

Each bundle contains three valid diagnostic records. Its log is byte-identical
to the standalone log, and the bundle manifest records the matching hash.
