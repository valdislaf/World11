> Historical upstream notes, imported with World11. Hardware measurements and
> Windows paths below describe the original environment, not this public build.
> Use the current commands in the root README for this standalone repository.

# Мир 11: карта и обзор

Обновлено 2026-09-18, исходная база `59d95dc`. Текущие изменения, визуальная
проверка и замеры описаны в [WORLD11_REVIEW.md](WORLD11_REVIEW.md).

## Как устроен мир

| Компонент | Файл / символ | Что важно |
| --- | --- | --- |
| Регистрация | `cpp/world/WorldCatalog.cpp` | Мир 11 без гравитации |
| Камера, порталы, время | `cpp/world/CppWorldGl33.cpp` | `kWorld11Portals`, `initColliders`, `beforeBeginFrame`, `groundContactAt` |
| Последовательность рендера | `cpp/render/gl33/Gl33WorldRenderer.cpp` | `render`: карта теней, skybox, дно, декор, рыба, портал, глубина воды, пузыри за водой, композиция воды, пузыри перед водой |
| Тени | `cpp/render/gl33/Gl33ShadowMap.*` | Направленный свет `kWorld11LightDirection`, карта 2048² вокруг камеры (80 м, привязка к текселям), PCF 3×3. Отбрасывают: ориентиры, кораллы, водоросли, рыба (с вырезом плавников). Принимают: дно, ориентиры, декор, рыба. Пузыри и портал теней не дают; дно только принимает. Затеняется лишь прямая составляющая (`uShadowStrength` 0.78) |
| Океан и framebuffer | `cpp/render/gl33/Gl33Renderer.cpp` | Сетка LOD вокруг камеры, глубина сцены, преломление/поглощение |
| Дно | `cpp/world/World11Seabed.hpp` | CPU-функция и GLSL; используется также генератором и рыбой |
| Волны | `cpp/world/World11WaterSurface.hpp`, `cpp/render/gl33/World11WaterWaves.hpp` | CPU/GLSL варианты должны оставаться согласованными |
| Размещение декора | `cpp/world/World11DecorGenerator.*` | Seed, stableId, чанки 32 м; водоросли, кораллы, пузыри |
| Рендер декора | `cpp/render/gl33/Gl33World11DecorRenderer.cpp` | `updateVisibleChunks`, `updateCoralInstances`, `selectCoralLods` |
| Коралловые меши | `cpp/render/gl33/World11CoralGeometry.*` | Процедурные варианты и LOD |
| Рыба | `cpp/render/gl33/Gl33World11FishRenderer.cpp` | Шесть траекторий, общий меш/текстура, масштаб и цвет, деформация хвоста, текстура `datasets/0x00000019.fget` |
| Движение рыбы | `cpp/world/World11FishTrajectory.*` | Фиксированный шаг 1/120 с, seed, ограничения дна и волн |

Skybox мира 11: `datasets/0x00000017.fget`. Номер ресурса не равен номеру мира:
не предполагать, что `0x00000011.fget` описывает мир 11.

## Текущее состояние

- `World11Environment.hpp`: одинаковые параметры экспоненциального тумана
  для рыбы, декора и ориентиров; направление света рыбы согласовано с декором.
- `World11Landmarks.hpp`: позиции арки (-16,-64), источника (-35,-62),
  колонии (34,-82), высота портала над дном. Ориентиры не меняют seed генератора.
- `Gl33WorldRenderer.cpp`: `drawWorld11Landmarks`, отдельный шероховатый меш
  камня с внешними нормалями. Колония мягко пульсирует, тёплые метки ведут назад.
- `Gl33World11DecorRenderer.cpp`: 60 пузырей источника в существующих двух
  проходах воды; кеш позиции камеры, переиспользование групп и пропуск одинаковых
  загрузок instance-буфера. Любая смена чанков/seed инвалидирует кеш.
- `CppWorldGl33.cpp`: потолок следует CPU-волнам с запасом радиуса камеры + 0.15 м;
  пол остаётся по CPU-дну. Арка имеет вписанные AABB, центр прохода открыт.
- `Gl33World11FishRenderer.cpp`: шесть независимых seed, единые GPU-ресурсы,
  масштабы 0.585–1.0; анимация и траектории продолжаются при неподвижной камере.

## Проверки и запуск

```powershell
.\scripts\ReviewWorld11.ps1
.\build-review\clang\horizongates_cpp.exe --world 11
```

Скрипт проверен: Release Clang, 3/3 CPU-тестов, GPU probe без ошибок OpenGL.
`tests/World11SceneProbe.cpp` сохраняет шесть ракурсов и CSV; проверяет реальный
`CppWorldGl33::tick` у поверхности (240 кадров), у дна и на переходах 11 → 6 → 11.
Выходной каталог: `build-review/after/`, вне git.

Probe — отдельная цель `world11_scene_probe`, не входит в CTest, требует GPU.
Камеры, время и маршруты зафиксированы в его исходнике. Замеры: 1280×800,
без MSAA/vsync/HUD; CPU submit и GPU timer раздельно. Это не FPS всей игры.
Ручной интерактивный облёт не выполнен. Изображения просмотрены из настоящего
OpenGL framebuffer, включая воду, рыбу, ориентиры и поднятый обратный портал.

При изменениях seed/LOD проверять неподвижную камеру, перемещение, границы чанков
и восстановление пустых/непустых групп. Для сравнения кадров фиксировать seed,
позицию/направление камеры и время. Старые EXE из других build-каталогов не использовать.
