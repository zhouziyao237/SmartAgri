# V2.1.7b Fix Note

## Root cause

V2.1.7a defined:

```powershell
function Invoke-Idf([string[]]$Args) { ... @Args }
```

`$args` is a PowerShell automatic variable. Reusing it as a parameter name caused the intended command array not to reach `idf.py` on the user's Windows PowerShell environment. The symptom was `idf.py` printing its Usage page three times while the wrapper still printed DONE.

## Fix

- Removed the `$Args` wrapper entirely.
- Calls are explicit: `idf.py set-target esp32c3`, `idf.py build`, `idf.py -p COMx -b 460800 flash`.
- Verify `sdkconfig` contains the requested IDF target.
- Verify `build/project_description.json` exists after build.
- Abort on any nonzero exit code.
- Added `idf_cli_check.ps1`.
