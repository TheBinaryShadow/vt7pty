# VT7Pty 0.5.0 Raw Windows 7 Results

These are the unmodified JSON, text, and diagnostic records returned for the [0.5.0 release acceptance](../../2026-10-02-0.5.0-release-acceptance.md). All 18 raw files were copied and verified byte for byte against the returned originals. The JSON records retain complete update inventories, test outputs, and machine identities. Temporary agent-rejection fixture directories are omitted; their outcomes are recorded in the JSON tests.

The generated `release-review.json` also remains here. It reports `Pass`, zero failures, and `PublicationApproved: false`; SHA-256: `25ed300546fc372fcfdcc8f6d56fd8c164b140d309c17ebb710679931afffebb`.

| File | SHA-256 |
| --- | --- |
| `windows7-esu-20261002-094853-diagnostic-bundle/diagnostic-manifest.json` | `0c904d95616e47f1b7ec0ddebc6af1335ee42a94d53bed6fe4d1e8d7b27d3c14` |
| `windows7-esu-20261002-094853-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-esu-20261002-094853-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `9156419064e20395b9dc3950f36d815a7361dba86c110a7c8ca1e2bd0cc40b3d` |
| `windows7-esu-20261002-094853-diagnostics.jsonl` | `9156419064e20395b9dc3950f36d815a7361dba86c110a7c8ca1e2bd0cc40b3d` |
| `windows7-esu-20261002-094853.json` | `e19a93221664e100b1fdf039636380395ec88ce18a2e3e4e7522965d9d00d368` |
| `windows7-esu-20261002-094853.txt` | `79f608161717356ce807fa5016a6e2b828b81807786075a6eb5d2d962246ca23` |
| `windows7-legacy-20261002-094451-diagnostic-bundle/diagnostic-manifest.json` | `c95d931f5dc4076aea40d96bcaddae686f2ad18e0644f7e275d3fb1f6db314ac` |
| `windows7-legacy-20261002-094451-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-legacy-20261002-094451-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `84eadabd47be3a213a14806e3a9c6e5e788efb048b87aaa232558d11db3559be` |
| `windows7-legacy-20261002-094451-diagnostics.jsonl` | `84eadabd47be3a213a14806e3a9c6e5e788efb048b87aaa232558d11db3559be` |
| `windows7-legacy-20261002-094451.json` | `f24bd96c144b5c7c62ebbd47aca636ba7f474f29a7e32fb55dd3224abf48683e` |
| `windows7-legacy-20261002-094451.txt` | `d6683831ee34081b85a791e2f81d4977375c4195b33105c2ff1bb78a4fbde75b` |
| `windows7-nonesu-20261002-094436-diagnostic-bundle/diagnostic-manifest.json` | `39acf075eaf5d371db49081c3ded109e778496e74bb467ce8ca4ab1a384b0c4f` |
| `windows7-nonesu-20261002-094436-diagnostic-bundle/README.txt` | `36f985ea5f396f786e6e218bf57d05cbfa8e7522eef1257cfa41efa629772c1a` |
| `windows7-nonesu-20261002-094436-diagnostic-bundle/vt7pty-diagnostics.jsonl` | `65c810ecb8222e6eba2b1b280c7a509151a3e702dba6eb7c8c3b81b6748b6641` |
| `windows7-nonesu-20261002-094436-diagnostics.jsonl` | `65c810ecb8222e6eba2b1b280c7a509151a3e702dba6eb7c8c3b81b6748b6641` |
| `windows7-nonesu-20261002-094436.json` | `19dacbdb52e00a1ff1a35bf60023f5a709bd11a13b776990fc70a955c3cb9e2a` |
| `windows7-nonesu-20261002-094436.txt` | `ef3db5ada7312f20b1951857e423c1c29342d72f7a148140a090943bfc98d7ea` |

Each diagnostic bundle contains three valid records, zero invalid lines, the matching source/API/protocol identity, and disabled input capture. Its log is byte-identical to the corresponding standalone log and matches the bundle manifest hash. Warning and error severities are intentional diagnostic transport fixtures.
