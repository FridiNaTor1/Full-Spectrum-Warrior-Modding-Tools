#!/usr/bin/env python3
"""
FridiNaTor's Full Spectrum Warrior Modding Kit

All-in-one GUI tool Full Spectrum Warrior PC modding:

- No downs limit (FSW.dll binary patch)
- No mission failures (patch [CLoseBehaviorDescriptor] active=1 -> 0 in all PAKs)
- 999 grenades / M203 / smoke grenades (patch rules in all PAKs)
- Replace GameSpy with OpenSpy (FSW.dll string patch)
- Edit Descriptors (visual descriptor editor per PAK)
- Edit Game Rules (visual section/field editor per PAK)
- Custom Resolution (edit/create Resolution.cfg)

Python 3 + PyQt5 Required to run
"""

import sys
import re
from pathlib import Path
import shutil

from PyQt5 import QtWidgets, QtCore, QtGui

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def show_error(parent, title, message):
    QtWidgets.QMessageBox.critical(parent, title, message)


def show_info(parent, title, message):
    QtWidgets.QMessageBox.information(parent, title, message)


def ensure_backup(path: Path):
    """
    Make a .bak backup next to the given file if one does not exist.
    """
    bak = path.with_suffix(path.suffix + ".bak")
    if not bak.exists():
        shutil.copy2(path, bak)


def find_chapters_dir(install_path: Path) -> Path:
    return install_path / "Chapters"


def iter_pak_files(chapters_dir: Path):
    if not chapters_dir.is_dir():
        return []
    return sorted(
        p for p in chapters_dir.iterdir()
        if p.is_file() and p.suffix.lower() == ".pak"
    )


def adjust_to_original_size(original: bytes, new: bytes) -> bytes:
    """
    Ensure the modified PAK data is exactly the same size as the original by
    sacrificing trailing NULs (or padding with extra NULs if we somehow shrink).

    This is only used for descriptor edits and CLoseBehavior active=0 patches,
    never for the rules/AMMO region (those use local padding logic).
    """
    orig_len = len(original)
    new_len = len(new)
    if new_len == orig_len:
        return new

    if new_len > orig_len:
        diff = new_len - orig_len
        i = new_len - 1
        while i >= 0 and new[i] == 0:
            i -= 1
        nul_run = new_len - i - 1
        if nul_run < diff:
            raise ValueError("Not enough trailing NUL bytes available to keep PAK size constant.")
        return new[: new_len - diff]
    else:
        diff = orig_len - new_len
        return new + b"\x00" * diff

# ---------------------------------------------------------------------------
# DLL patching
# ---------------------------------------------------------------------------

NO_DOWNS_ORIG = bytes.fromhex("8B 03 3B 45 14 7E 0B 5E 5D B0 01 5B 83 C4 08 C2 04 00")
NO_DOWNS_PATCH = bytes.fromhex("8B 03 3B 45 14 EB 0B 5E 5D B0 01 5B 83 C4 08 C2 04 00")

def patch_no_downs_limit(parent, dll_path: Path):
    if not dll_path.is_file():
        show_error(parent, "FSW.dll not found", f"Could not find FSW.dll at:\n{dll_path}")
        return
    data = dll_path.read_bytes()
    idx = data.find(NO_DOWNS_ORIG)
    if idx == -1:
        if NO_DOWNS_PATCH in data:
            show_info(parent, "Already patched", "No downs limit patch already applied.")
        else:
            show_error(parent, "Pattern not found",
                       "Could not find the downs-limit pattern in FSW.dll.\n"
                       "This may be an unsupported version.")
        return
    ensure_backup(dll_path)
    patched = bytearray(data)
    patched[idx:idx+len(NO_DOWNS_ORIG)] = NO_DOWNS_PATCH
    dll_path.write_bytes(bytes(patched))
    show_info(parent, "Success", "No downs limit patch applied to FSW.dll.")


def patch_gamespy_to_openspy(parent, dll_path: Path):
    if not dll_path.is_file():
        show_error(parent, "FSW.dll not found", f"Could not find FSW.dll at:\n{dll_path}")
        return
    data = dll_path.read_bytes()
    src = b"gamespy.com"
    dst = b"openspy.net"  # same length
    count = data.count(src)
    if count == 0:
        if dst in data:
            show_info(parent, "Already patched", "GameSpy -> OpenSpy patch already applied.")
        else:
            show_error(parent, "gamespy.com not found in FSW.dll",
                       "Could not find 'gamespy.com' in FSW.dll.\n"
                       "This may be an unsupported version.")
        return
    ensure_backup(dll_path)
    data = data.replace(src, dst)
    dll_path.write_bytes(data)
    show_info(parent, "Success", f"Replaced {count} occurrence(s) of gamespy.com with openspy.net.")

# ---------------------------------------------------------------------------
# PAK patching: no mission failures
# ---------------------------------------------------------------------------

BLOCK_RE = re.compile(
    br'\[CLoseBehaviorDescriptor\](.*?)/end',
    re.DOTALL | re.IGNORECASE
)
ACTIVE_RE = re.compile(
    br'(active\s*=\s*)1\b',
    re.IGNORECASE
)

def patch_no_mission_failures_in_pak(pak_path: Path) -> int:
    """
    For a single PAK: in [CLoseBehaviorDescriptor] blocks, set active=0 where it was 1.
    Returns number of blocks changed.
    """
    original = pak_path.read_bytes()
    data = original
    patched_blocks = 0

    def patch_block(match: re.Match) -> bytes:
        nonlocal patched_blocks
        body = match.group(1)
        patched_body, count = ACTIVE_RE.subn(lambda m: m.group(1) + b"0", body)
        if count > 0:
            patched_blocks += 1
        return b"[CLoseBehaviorDescriptor]" + patched_body + b"/end"

    new_data, _ = BLOCK_RE.subn(patch_block, data)
    if patched_blocks > 0:
        new_data = adjust_to_original_size(original, new_data)
        ensure_backup(pak_path)
        pak_path.write_bytes(new_data)
    return patched_blocks


def patch_no_mission_failures(parent, chapters_dir: Path):
    paks = iter_pak_files(chapters_dir)
    if not paks:
        show_error(parent, "No PAKs found", f"No .PAK files found in:\n{chapters_dir}")
        return
    total_blocks = 0
    for pak in paks:
        blocks = patch_no_mission_failures_in_pak(pak)
        total_blocks += blocks
    show_info(parent, "No mission failures",
              f"Patched {total_blocks} [CLoseBehaviorDescriptor] block(s) across {len(paks)} PAK(s).")

# ---------------------------------------------------------------------------
# PAK patching: 999 grenades / M203 / smoke (rules-local)
# ---------------------------------------------------------------------------

AMMO_KEYS = [
    "AMMO_M203_EASY",
    "AMMO_M203_HARD",
    "AMMO_SMOKEGRENADE_EASY",
    "AMMO_SMOKEGRENADE_HARD",
    "AMMO_HANDGRENADE_EASY",
    "AMMO_HANDGRENADE_HARD",
]

# ---------------------------------------------------------------------------
# Resolution.cfg editing
# ---------------------------------------------------------------------------

def edit_resolution(parent, install_path: Path):
    dlg = QtWidgets.QDialog(parent)
    dlg.setWindowTitle("Custom Resolution")
    layout = QtWidgets.QFormLayout(dlg)

    width_spin = QtWidgets.QSpinBox()
    width_spin.setRange(320, 10000)
    width_spin.setValue(1920)

    height_spin = QtWidgets.QSpinBox()
    height_spin.setRange(240, 10000)
    height_spin.setValue(1080)

    layout.addRow("Width:", width_spin)
    layout.addRow("Height:", height_spin)

    btn_box = QtWidgets.QDialogButtonBox(
        QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel
    )
    layout.addRow(btn_box)

    btn_box.accepted.connect(dlg.accept)
    btn_box.rejected.connect(dlg.reject)

    # Try to pre-load existing Resolution.cfg if present
    res_path = install_path / "Resolution.cfg"
    if res_path.is_file():
        try:
            content = res_path.read_text(encoding="latin-1").strip()
            parts = content.split()
            if len(parts) >= 2:
                w, h = int(parts[0]), int(parts[1])
                width_spin.setValue(w)
                height_spin.setValue(h)
        except Exception:
            pass

    if dlg.exec_() != QtWidgets.QDialog.Accepted:
        return

    w = width_spin.value()
    h = height_spin.value()
    line = f"{w} {h} 0\n"

    if res_path.exists():
        ensure_backup(res_path)
    res_path.write_text(line, encoding="latin-1")
    show_info(parent, "Resolution saved",
              f"Resolution.cfg written as:\n{line.strip()}")

# ---------------------------------------------------------------------------
# Descriptor editor
# ---------------------------------------------------------------------------

class DescriptorRecord:
    def __init__(self, index: int, desc_type: str, start: int, end: int, body_bytes: bytes):
        self.index = index
        self.desc_type = desc_type
        self.start = start
        self.end = end
        self.body = body_bytes  # bytes between header and /end
        self.key_values = {}    # canonical_key -> str
        self._parse_body()

    def _parse_body(self):
        body = self.body
        # Key = value or Key = "value", stopping at NULs
        kv_pattern = re.compile(
            rb'([A-Za-z0-9_]+)\s*=\s*(?:"([^"\r\n\x00]*)"|([^\s\r\n;"\x00]+))'
        )
        for m in kv_pattern.finditer(body):
            raw_key = m.group(1).decode("ascii", errors="ignore")
            key = raw_key[0].upper() + raw_key[1:] if raw_key else raw_key
            val_bytes = m.group(2) if m.group(2) is not None else m.group(3)
            val = val_bytes.decode("latin-1", errors="ignore")
            self.key_values[key] = val


class PakModel:
    DESC_PATTERN = re.compile(
        rb'\[(C\w+Descriptor)\](.*?)/end',
        re.DOTALL
    )

    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        self.descriptors = []
        self.options = {}  # key -> set(values)
        self.all_keys = set()
        self._parse()

    def _parse(self):
        index = 0
        for m in self.DESC_PATTERN.finditer(self.data):
            desc_type = m.group(1).decode("ascii", errors="ignore")
            start = m.start()
            end = m.end()
            body_bytes = m.group(2)
            rec = DescriptorRecord(index, desc_type, start, end, body_bytes)
            self.descriptors.append(rec)
            for k, v in rec.key_values.items():
                self.all_keys.add(k)
                self.options.setdefault(k, set()).add(v)
            index += 1

    def replace_key_in_descriptor(self, rec: DescriptorRecord, key: str, new_value: str):
        body = rec.body
        key_bytes = key.encode("ascii", errors="ignore")
        pattern = re.compile(
            rb'(' + re.escape(key_bytes) + rb'\s*=\s*)(?:"[^"\r\n\x00]*"|[^\s\r\n;"\x00]+)',
            re.IGNORECASE,
        )

        def repl(m):
            prefix = m.group(1)
            if (not new_value) or (" " in new_value) or ("=" in new_value):
                val_bytes = f'"{new_value}"'.encode("latin-1")
            else:
                val_bytes = new_value.encode("latin-1")
            return prefix + val_bytes

        new_body, count = pattern.subn(repl, body, count=1)
        if count == 0:
            if (not new_value) or (" " in new_value) or ("=" in new_value):
                add = f'\n{key} = "{new_value}"\n'.encode("latin-1")
            else:
                add = f'\n{key} = {new_value}\n'.encode("latin-1")
            new_body = body + add

        rec.body = new_body
        rec.key_values[key] = new_value
        self.options.setdefault(key, set()).add(new_value)

    def build_new_data(self) -> bytes:
        new = bytearray()
        pos = 0
        for rec in self.descriptors:
            new += self.data[pos:rec.start]
            header = f'[{rec.desc_type}]'.encode("ascii", errors="ignore")
            new += header + rec.body + b"/end"
            pos = rec.end
        new += self.data[pos:]
        return bytes(new)


class DescriptorEditorWidget(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.rec = None
        self.model = None
        self.options = {}

        self.scroll = QtWidgets.QScrollArea()
        self.scroll.setWidgetResizable(True)

        self.form_container = QtWidgets.QWidget()
        self.form_layout = QtWidgets.QFormLayout(self.form_container)
        self.form_layout.setFieldGrowthPolicy(QtWidgets.QFormLayout.AllNonFixedFieldsGrow)
        self.scroll.setWidget(self.form_container)

        self.combo_widgets = {}

        self.raw_edit = QtWidgets.QPlainTextEdit()
        self.raw_edit.setReadOnly(True)
        self.raw_edit.setLineWrapMode(QtWidgets.QPlainTextEdit.NoWrap)

        vbox = QtWidgets.QVBoxLayout(self)
        vbox.addWidget(self.scroll, stretch=1)
        vbox.addWidget(QtWidgets.QLabel("Raw descriptor body (read-only):"))
        vbox.addWidget(self.raw_edit, stretch=1)

    def clear_form(self):
        while self.form_layout.rowCount():
            self.form_layout.removeRow(0)
        self.combo_widgets.clear()

    def set_descriptor(self, rec: DescriptorRecord, model: PakModel):
        self.rec = rec
        self.model = model
        self.options = model.options if model else {}

        self.clear_form()
        if rec is None or model is None:
            self.raw_edit.setPlainText("")
            return

        priority = ["Name", "Script", "Unit", "Trigger", "Target", "WeaponType"]
        keys_in_rec = list(rec.key_values.keys())
        ordered_keys = []
        for pk in priority:
            if pk in rec.key_values and pk not in ordered_keys:
                ordered_keys.append(pk)
        for k in keys_in_rec:
            if k not in ordered_keys:
                ordered_keys.append(k)

        for key in ordered_keys:
            vals = sorted(self.options.get(key, []))
            combo = QtWidgets.QComboBox()
            combo.setEditable(True)
            for v in vals:
                if v is None:
                    continue
                combo.addItem(v)
            current = rec.key_values.get(key, "")
            if current and current not in vals:
                combo.insertItem(0, current)
                combo.setCurrentIndex(0)
            else:
                combo.setCurrentText(current)
            combo.currentTextChanged.connect(lambda text, k=key: self.on_value_changed(k, text))
            self.form_layout.addRow(f"{key}:", combo)
            self.combo_widgets[key] = combo

        self.raw_edit.setPlainText(rec.body.decode("latin-1", errors="ignore"))

    def on_value_changed(self, key: str, text: str):
        if self.rec is None or self.model is None:
            return
        self.model.replace_key_in_descriptor(self.rec, key, text)
        self.raw_edit.setPlainText(self.rec.body.decode("latin-1", errors="ignore"))


class DescriptorEditorWindow(QtWidgets.QMainWindow):
    def __init__(self, pak_path: Path, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Descriptor Editor - {pak_path.name}")
        self.resize(1100, 700)

        try:
            self.model = PakModel(pak_path)
        except Exception as e:
            show_error(self, "Error", f"Failed to load PAK:\n{e}")
            raise

        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self.list_widget = QtWidgets.QTreeWidget()
        self.list_widget.setHeaderLabels(["Name", "Type", "#"])
        self.list_widget.itemSelectionChanged.connect(self.on_selection_changed)

        self.editor_widget = DescriptorEditorWidget()
        splitter.addWidget(self.list_widget)
        splitter.addWidget(self.editor_widget)
        splitter.setStretchFactor(1, 1)

        self.setCentralWidget(splitter)
        self._populate_descriptor_list()
        self._create_menus()

    def _populate_descriptor_list(self):
        self.list_widget.clear()
        for rec in self.model.descriptors:
            name = rec.key_values.get("Name", "")
            item = QtWidgets.QTreeWidgetItem([name, rec.desc_type, str(rec.index)])
            item.setData(0, QtCore.Qt.UserRole, rec.index)
            self.list_widget.addTopLevelItem(item)
        self.list_widget.resizeColumnToContents(0)
        self.list_widget.resizeColumnToContents(1)
        self.list_widget.resizeColumnToContents(2)

    def _create_menus(self):
        menubar = self.menuBar()
        file_menu = menubar.addMenu("&File")
        save_act = QtWidgets.QAction("Save", self)
        save_act.triggered.connect(self.save)
        file_menu.addAction(save_act)

    def on_selection_changed(self):
        items = self.list_widget.selectedItems()
        if not items:
            self.editor_widget.set_descriptor(None, None)
            return
        idx = items[0].data(0, QtCore.Qt.UserRole)
        rec = self.model.descriptors[idx]
        self.editor_widget.set_descriptor(rec, self.model)

    def save(self):
        new_data = self.model.build_new_data()
        new_data = adjust_to_original_size(self.model.data, new_data)
        ensure_backup(self.model.path)
        self.model.path.write_bytes(new_data)
        show_info(self, "Saved", f"PAK saved:\n{self.model.path}")

# ---------------------------------------------------------------------------
# Game Rules editor (structured)
# ---------------------------------------------------------------------------

class RuleField:
    def __init__(self, section_name: str, key: str, value: str, line_index: int):
        self.section_name = section_name
        self.key = key
        self.value = value  # string after '=' (without newline)
        self.line_index = line_index
        # Filled by parser:
        self.prefix = ""
        self.newline = ""


class RuleSection:
    def __init__(self, name: str):
        self.name = name
        self.fields_order = []  # list of keys in order
        self.fields = {}        # key -> RuleField


class RulesModel:
    def __init__(self, pak_path: Path):
        self.pak_path = pak_path
        self.data = pak_path.read_bytes()
        self.rules_start, self.rules_end = self._locate_rules_region()
        if self.rules_start == -1:
            raise RuntimeError("Could not find [AI] / [Health] rules region.")
        region = self.data[self.rules_start:self.rules_end].decode("latin-1", errors="ignore")
        self.lines = region.splitlines(keepends=True)
        self.sections = []      # list[RuleSection]
        self.section_map = {}   # name -> RuleSection
        self._parse_lines()

    def _locate_rules_region(self):
        data = self.data
        start = data.find(b"[AI]")
        if start == -1:
            return -1, -1
        health = data.find(b"[Health]", start)
        if health == -1:
            return -1, -1
        nul_run = b"\x00" * 10
        pos = data.find(nul_run, health)
        end = pos if pos != -1 else len(data)
        return start, end

    def _get_or_create_section(self, name: str) -> RuleSection:
        sec = self.section_map.get(name)
        if sec is None:
            sec = RuleSection(name)
            self.section_map[name] = sec
            self.sections.append(sec)
        return sec

    def _parse_lines(self):
        current_section = None
        # Allow leading NULs before the section header (e.g. "\x00\x00\x00[Rules]")
        header_re = re.compile(r'[\s\x00]*\[([^\]]+)\]\s*')
        # Capture: prefix (incl. key, '=', spaces), key, value, newline
        kv_re = re.compile(r'(\s*([A-Za-z0-9_]+)\s*=\s*)(.*?)(\r?\n?)$')

        for idx, line in enumerate(self.lines):
            m = header_re.match(line)
            if m:
                name = m.group(1)
                current_section = self._get_or_create_section(name)
                continue
            if current_section is None:
                continue
            m2 = kv_re.match(line)
            if not m2:
                continue
            prefix = m2.group(1)
            key = m2.group(2)
            value = m2.group(3)
            newline = m2.group(4)
            field = RuleField(current_section.name, key, value, idx)
            field.prefix = prefix
            field.newline = newline
            current_section.fields[key] = field
            current_section.fields_order.append(key)

    def update_field(self, field: RuleField, new_value: str):
        field.value = new_value

    def build_new_data(self) -> bytes:
        """Rebuild the PAK bytes, adjusting only the NUL padding immediately
        after the rules region so the overall file size stays identical and
        everything after the padding keeps its original offsets.
        """
        orig = self.data
        orig_len = len(orig)
        old_region = orig[self.rules_start:self.rules_end]
        old_len = len(old_region)

        # Rebuild rules lines minimally: only replace the value part of known fields,
        # keeping original prefixes (spaces/tabs, key, '=' formatting) and newlines.
        new_lines = list(self.lines)
        for sec in self.sections:
            for key in sec.fields_order:
                field = sec.fields[key]
                idx = field.line_index
                prefix = field.prefix if field.prefix is not None else f"{field.key} = "
                newline = field.newline if field.newline is not None else ""
                new_lines[idx] = f"{prefix}{field.value}{newline}"

        new_region = "".join(new_lines).encode("latin-1", errors="ignore")

        # Find the NUL padding run immediately after the rules region in the original
        i = self.rules_end
        while i < orig_len and orig[i] == 0:
            i += 1
        pad_start = self.rules_end
        pad_end = i
        pad_len = pad_end - pad_start

        if pad_len <= 0:
            # No padding; fall back to naive replacement (but keep size check)
            new_data = bytearray()
            new_data += orig[:self.rules_start]
            new_data += new_region
            new_data += orig[self.rules_end:]
            if len(new_data) != len(orig):
                raise RuntimeError("Rules rebuild changed file size without padding.")
            return bytes(new_data)

        diff = len(new_region) - old_len
        if diff == 0:
            new_data = bytearray()
            new_data += orig[:self.rules_start]
            new_data += new_region
            new_data += orig[self.rules_end:]
            return bytes(new_data)
        elif diff > 0:
            if diff > pad_len:
                raise RuntimeError("Not enough NUL padding after rules to expand.")
            new_pad_len = pad_len - diff
            new_data = bytearray()
            new_data += orig[:self.rules_start]
            new_data += new_region
            new_data += b"\x00" * new_pad_len
            new_data += orig[pad_end:]
            return bytes(new_data)
        else:
            extra = -diff
            new_pad_len = pad_len + extra
            new_data = bytearray()
            new_data += orig[:self.rules_start]
            new_data += new_region
            new_data += b"\x00" * new_pad_len
            new_data += orig[pad_end:]
            return bytes(new_data)

# ---------------------------------------------------------------------------
# Color parsing and widgets
# ---------------------------------------------------------------------------

def parse_color(value: str):
    """Parse 'R, G, B' or 'R, G, B, A'. Returns (r, g, b, a, components) or None."""
    # Try RGBA first
    m = re.match(r'\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*', value)
    if m:
        r, g, b, a = [max(0, min(255, int(x))) for x in m.groups()]
        return r, g, b, a, 4
    # Then RGB
    m = re.match(r'\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*', value)
    if not m:
        return None
    r, g, b = [max(0, min(255, int(x))) for x in m.groups()]
    a = 255
    return r, g, b, a, 3


class ColorFieldWidget(QtWidgets.QWidget):
    valueChanged = QtCore.pyqtSignal(str)

    def __init__(self, initial: str, parent=None):
        super().__init__(parent)
        self._updating = False
        parsed = parse_color(initial)
        if parsed:
            r, g, b, a, comps = parsed
            self.rgba = (r, g, b, a)
            self.components = comps  # 3 for RGB, 4 for RGBA
        else:
            self.rgba = (255, 255, 255, 255)
            self.components = 4

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.edit = QtWidgets.QLineEdit(initial)
        self.btn = QtWidgets.QPushButton("Pick")
        self.slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.slider.setRange(0, 255)
        self.label = QtWidgets.QLabel("EXAMPLE")
        self.label.setFixedWidth(80)
        self.label.setAlignment(QtCore.Qt.AlignCenter)

        layout.addWidget(self.edit, 2)
        layout.addWidget(self.btn)
        layout.addWidget(self.slider)
        layout.addWidget(self.label)

        self.edit.textChanged.connect(self.on_text_changed)
        self.btn.clicked.connect(self.on_pick_color)
        self.slider.valueChanged.connect(self.on_alpha_changed)

        # If the original value was RGB only, disable the alpha slider
        self.slider.setEnabled(self.components == 4)

        self._apply_state_to_widgets()

    def _format_value(self):
        r, g, b, a = self.rgba
        if self.components == 3:
            return f"{r}, {g}, {b}"
        else:
            return f"{r}, {g}, {b}, {a}"

    def _apply_state_to_widgets(self):
        self._updating = True
        r, g, b, a = self.rgba
        text = self._format_value()
        self.edit.setText(text)
        if self.components == 4:
            self.slider.setValue(a)
        color = QtGui.QColor(r, g, b, a)
        pal = self.label.palette()
        pal.setColor(QtGui.QPalette.WindowText, color)
        self.label.setPalette(pal)
        self._updating = False
        self.valueChanged.emit(text)

    def on_text_changed(self, text: str):
        if self._updating:
            return
        parsed = parse_color(text)
        if parsed:
            r, g, b, a, comps = parsed
            self.rgba = (r, g, b, a)
            self.components = comps
            # If user typed only RGB, disable alpha slider; otherwise enable
            self.slider.setEnabled(self.components == 4)
            self._apply_state_to_widgets()
        else:
            # still emit raw text so model gets something
            self.valueChanged.emit(text)

    def on_pick_color(self):
        r, g, b, a = self.rgba
        color = QtGui.QColor(r, g, b, a)
        chosen = QtWidgets.QColorDialog.getColor(color, self, "Pick color")
        if not chosen.isValid():
            return
        # Keep existing alpha; only change RGB
        self.rgba = (chosen.red(), chosen.green(), chosen.blue(), self.rgba[3])
        self._apply_state_to_widgets()

    def on_alpha_changed(self, value: int):
        if self._updating or self.components != 4:
            return
        r, g, b, _ = self.rgba
        self.rgba = (r, g, b, value)
        self._apply_state_to_widgets()


class RulesEditorWidget(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.section = None
        self.model = None

        self.scroll = QtWidgets.QScrollArea()
        self.scroll.setWidgetResizable(True)

        self.form_container = QtWidgets.QWidget()
        self.form_layout = QtWidgets.QFormLayout(self.form_container)
        self.form_layout.setFieldGrowthPolicy(QtWidgets.QFormLayout.AllNonFixedFieldsGrow)
        self.scroll.setWidget(self.form_container)

        vbox = QtWidgets.QVBoxLayout(self)
        vbox.addWidget(self.scroll, stretch=1)

    def clear_form(self):
        while self.form_layout.rowCount():
            self.form_layout.removeRow(0)

    def set_section(self, section: RuleSection, model: RulesModel):
        self.section = section
        self.model = model
        self.clear_form()
        if section is None or model is None:
            return

        for key in section.fields_order:
            field = section.fields[key]
            key_lower = key.lower()
            is_color_key = key_lower.endswith("color") or key_lower.endswith("col")
            if is_color_key and parse_color(field.value):
                widget = ColorFieldWidget(field.value)
                widget.valueChanged.connect(lambda text, f=field: model.update_field(f, text))
            else:
                edit = QtWidgets.QLineEdit(field.value)
                edit.textChanged.connect(lambda text, f=field: model.update_field(f, text))
                widget = edit
            self.form_layout.addRow(f"{key}:", widget)


class RulesEditorWindow(QtWidgets.QMainWindow):
    def __init__(self, pak_path: Path, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Game Rules Editor - {pak_path.name}")
        self.resize(900, 700)

        try:
            self.model = RulesModel(pak_path)
        except Exception as e:
            show_error(self, "Error", f"{e}")
            self.close()
            return

        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self.list_widget = QtWidgets.QListWidget()
        for sec in self.model.sections:
            self.list_widget.addItem(sec.name)
        self.list_widget.currentRowChanged.connect(self.on_section_changed)

        self.editor_widget = RulesEditorWidget()

        splitter.addWidget(self.list_widget)
        splitter.addWidget(self.editor_widget)
        splitter.setStretchFactor(1, 1)
        self.setCentralWidget(splitter)

        toolbar = self.addToolBar("Rules")
        save_act = QtWidgets.QAction("Save", self)
        save_act.triggered.connect(self.save)
        toolbar.addAction(save_act)

        if self.model.sections:
            self.list_widget.setCurrentRow(0)

    def on_section_changed(self, row: int):
        if row < 0 or row >= len(self.model.sections):
            self.editor_widget.set_section(None, None)
            return
        sec = self.model.sections[row]
        self.editor_widget.set_section(sec, self.model)

    def save(self):
        new_data = self.model.build_new_data()
        if len(new_data) != len(self.model.data):
            show_error(self, "Size mismatch",
                       "Internal error: rebuilt rules data changed total PAK size.")
            return
        ensure_backup(self.model.pak_path)
        self.model.pak_path.write_bytes(new_data)
        show_info(self, "Saved", f"Rules saved into:\n{self.model.pak_path}")

# ---------------------------------------------------------------------------
# 999 ammo using RulesModel
# ---------------------------------------------------------------------------

def patch_ammo_999_in_pak(pak_path: Path) -> int:
    """Patch AMMO_* values to 999 inside the rules region only,
    using the same safe NUL-padding adjustment as the rules editor.
    """
    try:
        model = RulesModel(pak_path)
    except Exception:
        # If rules region not found, fall back to global text replacement (no size change)
        original = pak_path.read_bytes()
        text = original.decode("latin-1")
        changed = 0
        for key in AMMO_KEYS:
            pattern = re.compile(rf'({re.escape(key)}\s*=\s*)\d+', re.IGNORECASE)
            new_text, count = pattern.subn(r"\g<1>999", text)
            if count:
                text = new_text
                changed += count
        if changed:
            new_bytes = text.encode("latin-1")
            if len(new_bytes) != len(original):
                raise RuntimeError("Fallback ammo patch changed file size; aborting.")
            ensure_backup(pak_path)
            pak_path.write_bytes(new_bytes)
        return changed

    changed = 0
    for sec in model.sections:
        for key in list(sec.fields_order):
            if key in AMMO_KEYS:
                field = sec.fields[key]
                if field.value.strip() != "999":
                    model.update_field(field, "999")
                    changed += 1
    if changed:
        new_data = model.build_new_data()
        if len(new_data) != len(model.data):
            raise RuntimeError("RulesModel build_new_data changed total size unexpectedly.")
        ensure_backup(pak_path)
        model.pak_path.write_bytes(new_data)
    return changed


def patch_ammo_999(parent, chapters_dir: Path):
    paks = iter_pak_files(chapters_dir)
    if not paks:
        show_error(parent, "No PAKs found", f"No .PAK files found in:\n{chapters_dir}")
        return
    total_changes = 0
    for pak in paks:
        total_changes += patch_ammo_999_in_pak(pak)
    show_info(parent, "Ammo patched",
              f"Patched {total_changes} ammo value(s) across {len(paks)} PAK(s).")

# ---------------------------------------------------------------------------
# Main GUI
# ---------------------------------------------------------------------------

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FridiNaTor's Full Spectrum Warrior Modding Kit")
        self.resize(900, 520)

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)

        layout = QtWidgets.QVBoxLayout(central)

        title = QtWidgets.QLabel("FridiNaTor's Full Spectrum Warrior modding kit")
        font = title.font()
        font.setPointSize(font.pointSize() + 6)
        font.setBold(True)
        title.setFont(font)
        title.setAlignment(QtCore.Qt.AlignCenter)
        layout.addWidget(title)

        warning = QtWidgets.QLabel(
            '<span style="color:red; font-weight:bold;">'
            'Backup your game before doing anything! This tool will make .bak files '
            'of the originals, but it is always better to be safe than sorry!'
            '</span>'
        )
        warning.setWordWrap(True)
        warning.setAlignment(QtCore.Qt.AlignCenter)
        layout.addWidget(warning)

        # Install path chooser
        path_layout = QtWidgets.QHBoxLayout()
        path_label = QtWidgets.QLabel("Game Install Location:")
        self.path_edit = QtWidgets.QLineEdit()
        browse_btn = QtWidgets.QPushButton("Browse...")
        browse_btn.clicked.connect(self.browse_install)

        path_layout.addWidget(path_label)
        path_layout.addWidget(self.path_edit, stretch=1)
        path_layout.addWidget(browse_btn)
        layout.addLayout(path_layout)

        layout.addSpacing(10)

        grid = QtWidgets.QGridLayout()
        row = 0

        btn1 = QtWidgets.QPushButton("1. No downs limit")
        btn1.clicked.connect(self.do_no_downs)
        grid.addWidget(btn1, row, 0)

        btn2 = QtWidgets.QPushButton("2. No mission failures\n(except downs if #1 not used)")
        btn2.clicked.connect(self.do_no_mission_failures)
        grid.addWidget(btn2, row, 1)
        row += 1

        btn3 = QtWidgets.QPushButton("3. 999 grenades, M203,\n   and smoke grenades")
        btn3.clicked.connect(self.do_ammo_999)
        grid.addWidget(btn3, row, 0)

        btn4 = QtWidgets.QPushButton("4. Replace GameSpy with OpenSpy")
        btn4.clicked.connect(self.do_openspy)
        grid.addWidget(btn4, row, 1)
        row += 1

        btn5 = QtWidgets.QPushButton("5. Edit Descriptors")
        btn5.clicked.connect(self.do_edit_descriptors)
        grid.addWidget(btn5, row, 0)

        btn6 = QtWidgets.QPushButton("6. Edit Game Rules")
        btn6.clicked.connect(self.do_edit_rules)
        grid.addWidget(btn6, row, 1)
        row += 1

        btn7 = QtWidgets.QPushButton("7. Custom Resolution")
        btn7.clicked.connect(self.do_resolution)
        grid.addWidget(btn7, row, 0)

        layout.addLayout(grid)
        layout.addStretch(1)

    # Utility
    def get_install_path(self) -> Path | None:
        text = self.path_edit.text().strip()
        if not text:
            show_error(self, "Install path missing",
                       "Please enter the game install location.")
            return None
        path = Path(text)
        if not path.is_dir():
            show_error(self, "Invalid path",
                       f"The specified path is not a directory:\n{path}")
            return None
        return path

    def browse_install(self):
        directory = QtWidgets.QFileDialog.getExistingDirectory(
            self,
            "Select Full Spectrum Warrior install directory",
            "",
        )
        if directory:
            self.path_edit.setText(directory)

    # Button handlers
    def do_no_downs(self):
        install = self.get_install_path()
        if not install:
            return
        dll_path = install / "FSW.dll"
        patch_no_downs_limit(self, dll_path)

    def do_openspy(self):
        install = self.get_install_path()
        if not install:
           	return
        dll_path = install / "FSW.dll"
        patch_gamespy_to_openspy(self, dll_path)

    def do_no_mission_failures(self):
        install = self.get_install_path()
        if not install:
            return
        chapters = find_chapters_dir(install)
        patch_no_mission_failures(self, chapters)

    def do_ammo_999(self):
        install = self.get_install_path()
        if not install:
            return
        chapters = find_chapters_dir(install)
        patch_ammo_999(self, chapters)

    def pick_pak_from_install(self, install: Path, title: str) -> Path | None:
        chapters = find_chapters_dir(install)
        paks = iter_pak_files(chapters)
        if not paks:
            show_error(self, "No PAKs found",
                       f"No .PAK files found in:\n{chapters}")
            return None

        items = [p.name for p in paks]
        item, ok = QtWidgets.QInputDialog.getItem(
            self, title, "Select PAK:", items, 0, False
        )
        if not ok:
            return None
        idx = items.index(item)
        return paks[idx]

    def do_edit_descriptors(self):
        install = self.get_install_path()
        if not install:
            return
        pak = self.pick_pak_from_install(install, "Edit Descriptors")
        if not pak:
            return
        win = DescriptorEditorWindow(pak, self)
        win.show()

    def do_edit_rules(self):
        install = self.get_install_path()
        if not install:
            return
        pak = self.pick_pak_from_install(install, "Edit Game Rules")
        if not pak:
            return
        win = RulesEditorWindow(pak, self)
        win.show()

    def do_resolution(self):
        install = self.get_install_path()
        if not install:
            return
        edit_resolution(self, install)


def main():
    app = QtWidgets.QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec_()


if __name__ == "__main__":
    sys.exit(main())
