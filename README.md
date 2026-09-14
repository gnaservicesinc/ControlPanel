# ControlPanel

Two macOS desktop apps for making and using your own tabbed control panels.

- **Creator:** add, edit, reorder, duplicate, and remove tabs and buttons; choose
  actions and files; visualize the result; then export XML or a standalone app.
- **Interpreter:** load a panel and use its controls. Labels wrap inside an evenly
  spaced grid that adapts to the window width. Large panels scroll vertically.

## Build and open

Requires Apple Silicon (arm64), macOS 13+, Xcode or its command-line tools, Qt 6.8+ with Widgets and Test,
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

Applications target **arm64 only**. Bundled Qt frameworks and plugins are also
stripped to arm64; no Intel slices are shipped.
Copy the **apps from `dist/`** anywhere, including `/Applications`. The apps in
`build/` are intermediate development builds. Your own action programs/scripts
and target files remain at the paths you configure; exporting XML does not copy
those user files into the apps.

```sh
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
arm64 packages using Qt 6.8.3 as the supported baseline.

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

## Export a standalone control panel app

Use the **Creator from `dist/`**, which includes the export runtime and all Qt
libraries. End users need only the exported `.app` and the internal software/files
its buttons control. They do not need Creator, Interpreter, Qt, or the panel XML.
No compilation or Xcode project setup is needed for an export.

1. Name the panel and configure its controls.
2. Click **App Export Settings…** under the panel name (also in the File menu).
   Choose an optional ICNS, PNG, or JPEG icon; the version for the next export;
   optional support contact details; and optional About/version notes.
3. Leave **Advance version after each successful export** enabled for automatic
   increments. The default starts at `1.0.0`: the first app contains `1.0.0`, and
   the XML is then saved with `1.0.1` ready for the next export. Cancelling or
   failing an export does not advance the version. Disable this to reuse a version.
4. Choose **Export App…** from the toolbar or File menu. Save the source panel
   when asked, then choose where to create the app. A previous export from the
   same panel can be replaced; unrelated apps are preserved.
5. Open the resulting app. Its name and icon identify the panel, and its controls
   occupy the main window. There are no open, import, reload, or editor controls;
   a single tab fills the panel area without a redundant tab selector.

Versions accept one to three dot-separated numbers from 0 to 9999. The last
component increments, with carry at 9999. An exhausted version must be changed
or auto-increment disabled. An empty version stays empty and appears as **Not
set** in support information; macOS bundle metadata uses `1.0.0` as a fallback.
Export settings, including the selected signing certificate ID, are stored in
`<appExport>` in the source XML. Keep the selected icon available for re-export;
its contents are copied into each exported app.

### Support information and panel identity

End users can always click **Help / Support** at the bottom of the window, or
choose **Help → Support Information…** or **About [panel name]**. This shows the
app name, its exported version, **Panel ID (UUID)**, support contact, and About
notes. **Copy Support Information** copies the complete details for pasting into
an email or issue report. Support uses the version to identify the revision and
the UUID to identify the source panel, even when different panels have the same name.

Every newly created panel gets a UUID. Creator and Interpreter atomically add
one to older files on their first successful open; files missing a UUID must be
writable (or copied to a writable folder first). Read-only `--validate` does not
modify files. Reopening, renaming, Save As, and exporting preserve the UUID;
**New Panel** generates a different one. A copy of an existing XML file retains
its identity, so UUIDs identify the panel lineage, not a unique filesystem path.
Creator shows the UUID in App Export Settings.

### Packaging and local signing

The dedicated runtime contains a compact CBOR snapshot in a read-only Mach-O
section of the executable. It decodes that snapshot directly without reading or
parsing a panel XML file. The runtime is linked separately from the creator and exporter. It has no
external-panel loading interface, file associations, or Finder open handler.

The exporter signs the completed app with macOS `codesign`, using **Local use
(ad-hoc signing)** by default. Optionally select a development signing identity
already installed in the Mac's keychain, including identities set up with Xcode.
Signing and verification must succeed before publishing the app. The source
version update must also succeed; otherwise the previous app is restored.
No Developer ID account, notarization, or external distribution is required.
Exported apps contain arm64 code only, including their bundled Qt runtime.

On launch the app checks its signature, resources and nested code using Apple's
[Code Signing Services](https://developer.apple.com/documentation/security/secstaticcodecheckvalidity(_:_:_:)),
then checks the embedded snapshot's SHA-256 checksum before decoding it. This
provides tamper detection and barriers to accidental changes, not encryption or
protection against a machine owner who can patch and re-sign an app. Local
ad-hoc signing does not establish a publisher identity for another machine.
These apps are intended for a controlled internal environment.

Relative button paths are resolved to absolute paths at export time. `~/` still
resolves for the user running the app. The working directory remains the source
panel folder while it exists, otherwise it falls back to the running user’s home
folder so absolute programs still work without the authoring folder. Scripts that
rely on a working directory or relative arguments still need that folder, and
action targets must remain available. XML deletion has no effect on the panel. Programs, scripts, data files, and arbitrary files mentioned in
arguments are not bundled or covered by the app signature. Install/manage that
internal software separately. Buttons continue to run only when clicked.

For support tooling, exported apps accept `--support-info`, `--version`, and
`--verify`; these verify the embedded app and never run actions. All other
arguments, including panel filenames and Qt command-line overrides, are rejected.

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
  <uuid>708726dd-e23d-46c8-aa51-9ae210b304ea</uuid>
  <appExport>
    <version>1.0.0</version>
    <autoIncrement>true</autoIncrement>
    <iconPath></iconPath>
    <contact>Internal Help Desk</contact>
    <description></description>
    <signingIdentity>-</signingIdentity>
  </appExport>
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
`ToolTip`, `uuid`, and `appExport` (with the fields shown above). Legacy `<Value>` is accepted and exported as `<value>`. Unknown or
duplicate fields, unsupported actions, attributes, namespaces, DTDs and malformed
XML are rejected. Older files may omit `uuid` and `appExport`; their defaults
are added on migration/save. Older ControlPanel versions that do not understand
these fields cannot open the updated XML. Limits: 4 MiB, 100 tabs, 2,000 buttons.

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

The apps share C++17 libraries for the model, action runner and renderer. The
standalone runtime links only the runtime library, without creator/exporter UI.
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
and locks, safe previews, resizing/wrapping, and Creator → Interpreter execution. App-export coverage checks metadata/UUID
migration, binary decoding, support-info copying, settings UI, version increments,
real signed bundles, custom icons, filename rejection, failure preservation, and
tamper rejection. Packaging runs the bundle integration test against its signed
arm64 runtime.
For visual review, set `CP_SCREENSHOT_DIR` when running
`controlpanel_tests renderWindows`. Regenerate the checked-in app icons with
`./scripts/make-icons.sh` (Xcode Swift/AppKit and Apple `iconutil`).

GNU GPL v3. See [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md).
Qt integration follows its official [process API](https://doc.qt.io/qt-6/qprocess.html)
and [macOS deployment documentation](https://doc.qt.io/qt-6/macos-deployment.html).
