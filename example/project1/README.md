# Project One

Project One is a pure ssiLang desktop application sample.

It is intentionally split like a small real codebase instead of one script:

- `core/`
  Domain objects and state container
- `services/`
  Presentation shaping
- `ui/`
  Window construction, theme, and rendering
- `app/`
  Bootstrap and controller
- `main.sl`
  GUI entry point
- `smoke.sl`
  Non-GUI verification of the application core

## What it proves

This project is written entirely in `.sl` files.

The interpreter is unchanged.
The app uses the existing external `gui32` helper only as a runtime dependency, the same way a C++ application would depend on a UI library.

The project demonstrates:

- layered modules
- stateful store with controlled mutation
- separated rendering and controller logic
- reusable presenter/service layer
- a real event loop
- a desktop UI with list/detail editing, menu commands, status updates, and chart drawing

## Run the smoke script

```powershell
.\sl.exe example\project1\smoke.sl
```

## Run the GUI smoke script

Start the helper first:

```powershell
.\tools\gui32_helper.exe
```

Then run:

```powershell
.\sl.exe example\project1\gui_smoke.sl
```

## Run the GUI app

Start the helper first:

```powershell
.\tools\gui32_helper.exe
```

Then run:

```powershell
.\sl.exe example\project1\main.sl
```

## Layout

Project One is a small delivery desk application:

- left: task list
- right: editable task details
- bottom-right: delivery snapshot canvas
- bottom: status line
- top menu: file/help actions

## Project tree

```text
example/project1/
  app/
    bootstrap.sl
    controller.sl
  core/
    sample_data.sl
    store.sl
    task.sl
  services/
    presenter.sl
  ui/
    main_window.sl
    render.sl
    theme.sl
  gui_smoke.sl
  main.sl
  smoke.sl
  README.md
```
