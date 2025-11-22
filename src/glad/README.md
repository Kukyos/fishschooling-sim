GLAD loader is NOT included in this cleaned folder.

To generate GLAD files:
1. Visit https://glad.dav1d.de/
2. Language: C/C++
3. API: gl=4.5 (or higher), Profile: core, Loader: Yes
4. Generate, download, and run `install_glad.ps1` (or copy files manually):
   - `glad.c` -> `src/glad.c`
   - `glad/glad.h` -> `src/glad/glad.h`
   - `KHR/khrplatform.h` -> `src/KHR/khrplatform.h`

Alternatively, run the provided `install_glad.ps1` script from the `Final Cleaned` folder.
