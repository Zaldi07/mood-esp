from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Optional

from PyQt6.QtCore import QRect, Qt, pyqtSignal
from PyQt6.QtGui import QAction, QColor, QPainter, QPen
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

OLED_WIDTH = 128
OLED_HEIGHT = 64
PREVIEW_SCALE = 4
DEFAULT_HEADER = Path("/Users/jzh/Projects/carmood/firmware/main/ui/layout_default_data.h")


@dataclass
class LayoutItem:
    type: str
    x: int = 0
    y: int = 0
    w: int = 20
    h: int = 10
    x2: int = 0
    y2: int = 0
    scale: int = 1
    filled: bool = False
    text: str = ""


class PreviewWidget(QWidget):
    def __init__(self, get_items):
        super().__init__()
        self._get_items = get_items
        self.setMinimumSize(OLED_WIDTH * PREVIEW_SCALE, OLED_HEIGHT * PREVIEW_SCALE)

    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor("#e8e8e8"))
        panel = QRect(0, 0, OLED_WIDTH * PREVIEW_SCALE, OLED_HEIGHT * PREVIEW_SCALE)
        painter.fillRect(panel, QColor("white"))
        painter.setPen(QPen(QColor("black"), 1))
        painter.drawRect(panel.adjusted(0, 0, -1, -1))

        for item in self._get_items():
            if item.type == "text":
                painter.drawText(
                    item.x * PREVIEW_SCALE,
                    item.y * PREVIEW_SCALE + 8 * max(item.scale, 1),
                    item.text or "TEXT",
                )
            elif item.type == "line":
                painter.drawLine(
                    item.x * PREVIEW_SCALE,
                    item.y * PREVIEW_SCALE,
                    item.x2 * PREVIEW_SCALE,
                    item.y2 * PREVIEW_SCALE,
                )
            elif item.type == "rect":
                rect = QRect(
                    item.x * PREVIEW_SCALE,
                    item.y * PREVIEW_SCALE,
                    item.w * PREVIEW_SCALE,
                    item.h * PREVIEW_SCALE,
                )
                if item.filled:
                    painter.fillRect(rect, QColor("black"))
                else:
                    painter.drawRect(rect)


class PropertyEditor(QWidget):
    changed = pyqtSignal()

    def __init__(self):
        super().__init__()
        self._item: Optional[LayoutItem] = None

        self.text = QLineEdit()
        self.x = self._spin(0, OLED_WIDTH)
        self.y = self._spin(0, OLED_HEIGHT)
        self.w = self._spin(0, OLED_WIDTH)
        self.h = self._spin(0, OLED_HEIGHT)
        self.x2 = self._spin(0, OLED_WIDTH)
        self.y2 = self._spin(0, OLED_HEIGHT)
        self.scale = self._spin(1, 4)
        self.filled = QCheckBox()

        form = QFormLayout(self)
        form.addRow("Text", self.text)
        form.addRow("X", self.x)
        form.addRow("Y", self.y)
        form.addRow("W", self.w)
        form.addRow("H", self.h)
        form.addRow("X2", self.x2)
        form.addRow("Y2", self.y2)
        form.addRow("Scale", self.scale)
        form.addRow("Filled", self.filled)

        self.text.textChanged.connect(self._apply)
        for widget in (self.x, self.y, self.w, self.h, self.x2, self.y2, self.scale):
            widget.valueChanged.connect(self._apply)
        self.filled.toggled.connect(self._apply)

    def _spin(self, min_val: int, max_val: int) -> QSpinBox:
        spin = QSpinBox()
        spin.setRange(min_val, max_val)
        return spin

    def set_item(self, item: Optional[LayoutItem]):
        self._item = item
        enabled = item is not None
        for child in (self.text, self.x, self.y, self.w, self.h, self.x2, self.y2, self.scale, self.filled):
            child.setEnabled(enabled)

        if item is None:
            self.text.clear()
            return

        self.text.setText(item.text)
        self.x.setValue(item.x)
        self.y.setValue(item.y)
        self.w.setValue(item.w)
        self.h.setValue(item.h)
        self.x2.setValue(item.x2)
        self.y2.setValue(item.y2)
        self.scale.setValue(item.scale)
        self.filled.setChecked(item.filled)

    def _apply(self):
        if self._item is None:
            return
        self._item.text = self.text.text()
        self._item.x = self.x.value()
        self._item.y = self.y.value()
        self._item.w = self.w.value()
        self._item.h = self.h.value()
        self._item.x2 = self.x2.value()
        self._item.y2 = self.y2.value()
        self._item.scale = self.scale.value()
        self._item.filled = self.filled.isChecked()
        self.changed.emit()


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Layout Studio")
        self.items: list[LayoutItem] = []

        self.list_widget = QListWidget()
        self.editor = PropertyEditor()
        self.preview = PreviewWidget(lambda: self.items)

        self.editor.changed.connect(self.refresh)
        self.list_widget.currentRowChanged.connect(self._on_select)

        left = QVBoxLayout()
        left.addWidget(QLabel("Elements"))
        left.addWidget(self.list_widget)

        add_text = QPushButton("Add Text")
        add_line = QPushButton("Add Line")
        add_rect = QPushButton("Add Rect")
        delete = QPushButton("Delete")
        add_text.clicked.connect(lambda: self.add_item("text"))
        add_line.clicked.connect(lambda: self.add_item("line"))
        add_rect.clicked.connect(lambda: self.add_item("rect"))
        delete.clicked.connect(self.delete_item)

        button_row = QHBoxLayout()
        button_row.addWidget(add_text)
        button_row.addWidget(add_line)
        button_row.addWidget(add_rect)
        button_row.addWidget(delete)
        left.addLayout(button_row)

        center = QVBoxLayout()
        center.addWidget(QLabel("Preview"))
        center.addWidget(self.preview)

        right = QVBoxLayout()
        right.addWidget(QLabel("Properties"))
        right.addWidget(self.editor)

        root = QWidget()
        layout = QHBoxLayout(root)
        layout.addLayout(left, 1)
        layout.addLayout(center, 1)
        layout.addLayout(right, 1)
        self.setCentralWidget(root)

        self._build_toolbar()
        self._seed_demo()
        self.refresh()

    def _build_toolbar(self):
        toolbar = QToolBar("Actions")
        self.addToolBar(toolbar)

        open_action = QAction("Open JSON", self)
        save_action = QAction("Save JSON", self)
        export_action = QAction("Export C Header", self)
        export_default_action = QAction("Export Default Header", self)

        open_action.triggered.connect(self.open_json)
        save_action.triggered.connect(self.save_json)
        export_action.triggered.connect(self.export_header)
        export_default_action.triggered.connect(lambda: self.export_header(DEFAULT_HEADER))

        toolbar.addAction(open_action)
        toolbar.addAction(save_action)
        toolbar.addAction(export_action)
        toolbar.addAction(export_default_action)

    def _seed_demo(self):
        self.items = [
            LayoutItem(type="text", x=8, y=8, text="CARMOOD", scale=1),
            LayoutItem(type="line", x=8, y=18, x2=119, y2=18),
            LayoutItem(type="text", x=12, y=28, text="12:45", scale=3),
            LayoutItem(type="text", x=12, y=52, text="LAYOUT PAGE", scale=1),
            LayoutItem(type="rect", x=94, y=28, w=24, h=18),
        ]

    def add_item(self, item_type: str):
        item = LayoutItem(type=item_type)
        if item_type == "text":
            item.text = "TEXT"
        elif item_type == "line":
            item.x2 = 40
            item.y2 = 10
        self.items.append(item)
        self.refresh(select_index=len(self.items) - 1)

    def delete_item(self):
        row = self.list_widget.currentRow()
        if row < 0:
            return
        del self.items[row]
        self.refresh(select_index=min(row, len(self.items) - 1))

    def refresh(self, select_index: Optional[int] = None):
        current = self.list_widget.currentRow() if select_index is None else select_index
        self.list_widget.blockSignals(True)
        self.list_widget.clear()
        for idx, item in enumerate(self.items):
            label = f"{idx + 1}. {item.type}"
            if item.type == "text" and item.text:
                label += f" [{item.text}]"
            self.list_widget.addItem(QListWidgetItem(label))
        self.list_widget.blockSignals(False)

        if 0 <= current < len(self.items):
            self.list_widget.setCurrentRow(current)
        else:
            self.editor.set_item(None)
        self.preview.update()

    def _on_select(self, row: int):
        self.editor.set_item(self.items[row] if 0 <= row < len(self.items) else None)
        self.preview.update()

    def open_json(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open Layout JSON", "", "JSON Files (*.json)")
        if not path:
            return
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        self.items = [LayoutItem(**item) for item in data.get("items", [])]
        self.refresh(select_index=0 if self.items else None)

    def save_json(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save Layout JSON", "", "JSON Files (*.json)")
        if not path:
            return
        data = {"items": [asdict(item) for item in self.items]}
        Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")

    def export_header(self, forced_path: Optional[Path] = None):
        if forced_path is None:
            path_str, _ = QFileDialog.getSaveFileName(self, "Export C Header", "", "Header Files (*.h)")
            if not path_str:
                return
            target = Path(path_str)
        else:
            target = forced_path

        target.write_text(self._build_header(), encoding="utf-8")
        QMessageBox.information(self, "Exported", f"Header written to:\n{target}")

    def _build_header(self) -> str:
        lines = [
            "#pragma once",
            "",
            '#include "ui/layout_renderer.h"',
            "",
            "static const layout_item_t s_default_layout_items[] = {",
        ]
        for item in self.items:
            lines.append("    {")
            item_type = {
                "text": "LAYOUT_ITEM_TEXT",
                "line": "LAYOUT_ITEM_LINE",
                "rect": "LAYOUT_ITEM_RECT",
            }[item.type]
            lines.append(f"        .type = {item_type},")
            lines.append(f"        .x = {item.x},")
            lines.append(f"        .y = {item.y},")
            if item.type == "text":
                lines.append(f"        .scale = {item.scale},")
                lines.append(f'        .text = "{item.text}",')
            elif item.type == "line":
                lines.append(f"        .x2 = {item.x2},")
                lines.append(f"        .y2 = {item.y2},")
            elif item.type == "rect":
                lines.append(f"        .w = {item.w},")
                lines.append(f"        .h = {item.h},")
                lines.append(f"        .filled = {'true' if item.filled else 'false'},")
            lines.append("    },")
        lines.extend(
            [
                "};",
                "",
                "static const layout_page_t s_default_layout_page = {",
                '    .name = "default",',
                "    .items = s_default_layout_items,",
                "    .item_count = sizeof(s_default_layout_items) / sizeof(s_default_layout_items[0]),",
                "};",
                "",
            ]
        )
        return "\n".join(lines)


def main():
    app = QApplication([])
    window = MainWindow()
    window.resize(1200, 520)
    window.show()
    app.exec()


if __name__ == "__main__":
    main()
