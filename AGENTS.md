# World11 agent instructions

Read README.md, docs/PROJECT_MAP.md, then docs/WORLD11.md before editing.
Check `git status --short`; preserve unrelated work. C++17 / OpenGL 3.3 Core / GLFW.

- Work on one agreed Issue in a fork and topic branch. Link the Issue in the PR.
- State which files you intend to change and coordinate overlapping tasks in the Issue.
- Keep changes within the task. Preserve stable seeds and CPU/GLSL agreement for seabed and waves.
- Preserve the ocean/hub portal round trip and the asset format and identifiers.
- Do not introduce private dependencies or commit build output, credentials or machine-specific paths.
- Follow nearby style. Document new public header APIs using the existing XML comment style.
- Build with CMake and run CTest; report exact commands and results in the PR.
- Rendering changes also require reproducible before/after images. CPU tests alone do not establish visual correctness.
- Use the optional world11_scene_probe target for screenshots and runtime checks. State renderer and environment.
- Never claim unperformed checks. Explain any remaining limitation.
- Submit a PR; merging is controlled by the repository owner. Repository instructions do not grant an agent permission to merge.

GitHub Issues are the task queue. This file does not start agents or send notifications.
