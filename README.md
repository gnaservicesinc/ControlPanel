# ControlPanel

Two macOS desktop apps for making and using your own tabbed control panels.

- **Creator:** add, edit, reorder, duplicate, and remove tabs and buttons; choose
  actions and files; visualize the result; then export XML.
- **Interpreter:** load a panel and use its controls. Labels wrap inside an evenly
  spaced grid that adapts to the window width. Large panels scroll vertically.

## Build and open

Requires macOS 13+, Xcode or its command-line tools, Qt 6.8+ with Widgets and Test,
and CMake 3.24+. The default Qt path is `/opt/Qt/6.11.2/macos`. CMake is found in
the Qt installer's Tools directory or `/Applications/CMake.app`.

**Double-click `Build.command` in Finder**, or run:

```sh
./build.sh
```

The default build compiles both apps, runs tests, **bundles Qt frameworks and
plugins inside each app**, verifies the bundles, and produces a ZIP and SHA-256
checksum in `dist/`. Packaged apps remain independent of the installed Qt version:
updating or removing that Qt installation does not change their runtime.

After a double-click build, Finder opens the output folder. From a terminal:

```sh
open "dist/ControlPanel-macos-arm64/ControlPanel Creator.app"
open "dist/ControlPanel-macos-arm64/ControlPanel Interpreter.app"
```

The suffix follows the build architecture: `arm64`, `x86_64`, or `universal`.
Copy the **apps from `dist/`** anywhere, including `/Applications`. The apps in
`build/` are intermediate development builds. Your own action programs/scripts
and target files remain at the paths you configure; exporting XML does not copy
those user files into the apps.

```sh
./build.sh --arch universal                 # Apple Silicon + Intel
./build.sh --qt /path/to/Qt/macos
./build.sh --cmake /absolute/path/to/cmake
./build.sh --build-dir build-dev --no-package
./build.sh --help
```

`CONTROLPANEL_QT_ROOT` and `CONTROLPANEL_CMAKE` also select those tools. Use a new
build directory when changing compilers or generators. Build products are ignored
by Git. Bundles are ad-hoc signed, not Developer ID signed or notarized.

### Controlled environment

The helper does not source shell startup files. All compilation, tests and
packaging run under `env -i`, using Xcode's compiler/SDK, an explicit CMake,
`/usr/bin/make`, and the selected Qt prefix. Inherited compiler/linker flags,
`DYLD_*`, `QT_*`, Python setup, and package-manager environment are cleared.
CMake package registries are disabled; `/usr/local`, `/opt/homebrew`, and
`/opt/local` are excluded from dependency discovery. The build PATH contains only
Apple system directories; CMake and CTest are invoked by absolute path.

Bundle verification checks embedded frameworks/plugins, every Mach-O dependency
and architecture, runtime search paths, minimum OS versions, and signatures. It
rejects references to build-machine libraries. GitHub Actions builds and uploads
universal packages using Qt 6.8.3 as the supported baseline.

## Create a panel

1. Enter a panel name in Creator.
2. Use **+ Tab** and **+ Button**; select an item to edit its properties.
3. Enter a button label, action, and file path. The file chooser can select a new
   destination for touch/counter actions.
4. Add run arguments, a counter increment, or a tooltip as needed.
5. Check the live preview. **Preview Window** opens a resizable snapshot. Neither
   preview runs actions.
6. **Save** or **Export for Interpreter** writes the same validated XML format.

Creator reopens exported files. It reports invalid fields before saving, saves
atomically, and prompts about unsaved changes when opening, creating or closing
a panel. Removing a nonempty tab requires confirmation.

## Use a panel

Use **File → Open Panel**, pass a file as a command-line argument, or use Finder's
**Open With**. **File → Reload Panel** picks up saved edits. The panel name becomes
the window title. Loading never executes actions. Open panels you trust: clicking
a control runs a program or changes its configured file with your permissions.

The status bar reports results. **View → Activity** shows exit codes and the last
16 KiB of combined process output; it opens automatically on errors. Processes
run asynchronously, with up to 16 simultaneous actions and no interactive stdin.
Closing waits for active actions. **Actions → Stop Running Actions** terminates
launched processes and force-kills them after 1.5 seconds if necessary. Programs
that spawn independent child processes are responsible for those children.

Try `examples/Getting Started.controlpanel`. Its buttons create markers and a
counter in your home folder, show the date, or run `hello.sh`. Keep the example
panel and script together. Files change only when their buttons are clicked.

## XML format

Use `.controlpanel` or `.xml`; both contain UTF-8 XML:

```xml
<ControlPanel>
  <name>My Control Panel</name>
  <Tab>
    <name>Counter</name>
    <button>
      <name>Increase alarm</name>
      <action>storeincrement</action>
      <path>~/alarm_level.txt</path>
      <value>1</value>
      <ToolTip>Add one to the alarm level.</ToolTip>
    </button>
  </Tab>
</ControlPanel>
```

Panel/tab names and button names/actions/paths are required. A panel needs at
least one named tab; tabs may be empty. Element names are case-sensitive:
`ControlPanel`, `Tab`, `button`, `name`, `action`, `path`, `args`, `value`, and
`ToolTip`. Legacy `<Value>` is accepted and exported as `<value>`. Unknown or
duplicate fields, unsupported actions, attributes, namespaces, DTDs and malformed
XML are rejected. Limits: 4 MiB, 100 tabs, 2,000 buttons.

| Action | Behavior | Optional fields |
| --- | --- | --- |
| `touch` | Runs `/usr/bin/touch` with the resolved path as one argument. Creates a missing file or updates its timestamp. | `ToolTip` |
| `storeincrement` | Reads the first whitespace-delimited number, adds the increment, and writes the result plus a newline. | `value` (default `1`), `ToolTip` |
| `run` | Runs the executable at the resolved path directly. | `args`, `ToolTip` |

### Paths and arguments

Paths can be absolute, relative to the **saved panel's folder**, or start with
`~/`. Relative paths follow the new directory after Save As. Parent folders must
exist. Path environment variables are not expanded.

Scripts need execute permission and a valid shebang such as `#!/bin/sh`:
`chmod +x myscript.sh`. Alternatively, choose `/bin/sh` as the program and supply
the script path in its arguments. Working directory is the panel's folder. Child
PATH is `/usr/bin:/bin:/usr/sbin:/sbin`, consistently across Finder and Terminal.
Use absolute paths or set PATH inside scripts for tools installed elsewhere.
Other environment variables are inherited except `DYLD_*`, `QT_*`, and `QML_*`.

Arguments split on whitespace. Single/double quotes preserve spaces, empty
quotes produce an empty argument, and backslash escapes the next character
outside single quotes. For example:

```xml
<args>47 "two words" '' 'literal $HOME'</args>
```

This supplies four arguments. No shell, wildcard, variable, redirect, pipe, or
command substitution is applied. To intentionally use a shell, configure that
shell as the program. Creator handles XML escaping such as `&amp;` automatically.

### Counters

A missing, unreadable, empty or nonnumeric file starts at zero. The first
whitespace-delimited signed decimal/scientific number is used (`level 4.5 99`
uses `4.5`). The entire file is replaced with the new value. Files over 64 KiB
are rejected without changes. Numbers use C-locale decimal points and finite
double precision. Overflow and increments too small to change the value fail
without writing.

A sibling `.controlpanel.lock` serializes ControlPanel updates; a busy lock
reports an error immediately so you can retry. Symlinks resolve to their target
before locking/writing. Atomic writes require a writable destination directory.
Unrelated programs must cooperate with this lock to avoid concurrent-write races.

## Development

Both apps share a C++17 library for the XML model, action runner and renderer.
The ordinary CMake workflow is available too:

```sh
cmake -S . -B build-custom -DCMAKE_PREFIX_PATH=/path/to/Qt/macos
cmake --build build-custom --parallel
ctest --test-dir build-custom --output-on-failure
```

Direct CMake builds do not sanitize the environment or package Qt; use `build.sh`
for distributable apps. The core/widgets code also builds on Qt-supported Unix
systems; the packaging helper targets macOS.

Both apps support non-executing command-line validation:

```sh
"build/ControlPanel Interpreter.app/Contents/MacOS/ControlPanel Interpreter" \
  --validate "examples/Getting Started.controlpanel"
```

Exit codes: `0` valid, `1` invalid, `2` bad usage. Tests cover XML round trips,
malformed input, quoting, real process execution/failures/cancellation, counters
and locks, safe previews, resizing/wrapping, and Creator → Interpreter execution.
For visual review, set `CP_SCREENSHOT_DIR` when running
`controlpanel_tests renderWindows`. Regenerate the checked-in app icons with
`./scripts/make-icons.sh` (Xcode Swift/AppKit and Apple `iconutil`).

GNU GPL v3. See [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md).
Qt integration follows its official [process API](https://doc.qt.io/qt-6/qprocess.html)
and [macOS deployment documentation](https://doc.qt.io/qt-6/macos-deployment.html).
