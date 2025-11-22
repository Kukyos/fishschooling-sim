# Publishing & GitHub Guidelines

This folder contains a cleaned, minimal version of the CUDA Fish Schooling project. It's ready to be added to a GitHub repository.

Recommended commit structure:
- `Initial commit`: Add sources and scripts
- `Add README & build scripts`
- `Add install script for GLAD & dependency notes`

Suggested `.gitignore` file is included.

Publishing steps:
```powershell
cd "Final Cleaned"
# (Optional) Create a new repo on GitHub, then:
git init
git add .
git commit -m "Initial cleaned project: CUDA fish schooling visualizer"
# Add remote, then push
git remote add origin <your_git_repo_here>
git branch -M main
git push -u origin main
```

Notes for downstream users:
- This cleaned folder does not contain large assets or engine projects (UE5), which should remain in the original repo if needed.
- The project requires external dependencies (GLFW, GLM, ImGui, GLAD, CUDA) that can either be installed system-wide or added as submodules.
- Use `install_glad.ps1` to generate GLAD files (requires `glad.zip` downloaded from glad.dav1d.de).
- For reproduction, include the original `GPU` or `UE5` project in a different repo or as submodules if desired.

License: add a LICENSE file at repository root if you want to use a permissive license like MIT.
