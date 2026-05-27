"""
VOFA+ 数据分析器
- 使用 pyqtgraph 绘图
- 十字交叉线追踪鼠标
"""

import os
import numpy as np
import pandas as pd
import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets, QtCore, QtGui

# ==================== 绘图样式配置 ====================

LINE_WIDTH = 1.5
LINE_STYLE = 'solid'  # 'solid', 'dash', 'dot', 'dashdot', 'dashdotdot'
FILL_ENABLED = False
FILL_ALPHA = 80

# 默认颜色列表
DEFAULT_COLORS = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b']

# 线型映射
_PEN_STYLE_MAP = {
    'solid': QtCore.Qt.PenStyle.SolidLine if hasattr(QtCore.Qt, 'PenStyle') else QtCore.Qt.SolidLine,
    'dash': QtCore.Qt.PenStyle.DashLine if hasattr(QtCore.Qt, 'PenStyle') else QtCore.Qt.DashLine,
    'dot': QtCore.Qt.PenStyle.DotLine if hasattr(QtCore.Qt, 'PenStyle') else QtCore.Qt.DotLine,
    'dashdot': QtCore.Qt.PenStyle.DashDotLine if hasattr(QtCore.Qt, 'PenStyle') else QtCore.Qt.DashDotLine,
    'dashdotdot': QtCore.Qt.PenStyle.DashDotDotLine if hasattr(QtCore.Qt, 'PenStyle') else QtCore.Qt.DashDotDotLine,
}


class VofaAnalyzer:
    """VOFA+ 数据分析器"""

    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._reset()
        return cls._instance

    def _reset(self):
        """初始化/重置状态"""
        self.channel0_is_timestamp = True
        self.csv_filepath = 'vofa+.csv'
        self._timestamp = None
        self._data = None
        self._fs = None
        self._channels = []

    def load_data(self):
        """加载 CSV 数据"""
        filepath = self.csv_filepath
        if not os.path.isabs(filepath):
            filepath = os.path.join(os.path.dirname(__file__), filepath)

        df = pd.read_csv(filepath)

        if self.channel0_is_timestamp:
            self._timestamp = df.iloc[:, 0].values.astype(np.float64)
            self._data = df.iloc[:, 1:].values

            # 检测时间戳是否递增，处理异常数据
            ts_diff = np.diff(self._timestamp)
            if np.any(ts_diff < 0):
                # 找到时间戳跳变的位置（负跳变说明有异常数据）
                jump_idx = np.where(ts_diff < 0)[0]
                if len(jump_idx) > 0:
                    # 跳过异常数据之前的部分
                    start_idx = jump_idx[-1] + 1
                    self._timestamp = self._timestamp[start_idx:]
                    self._data = self._data[start_idx:]
                    ts_diff = np.diff(self._timestamp)

            # 计算采样频率（使用有效差值的中位数，更鲁棒）
            valid_diff = ts_diff[ts_diff > 0]
            if len(valid_diff) > 0:
                median_diff = np.median(valid_diff)
                self._fs = 1.0 / (median_diff / 1e6)  # 微秒转秒
            else:
                self._fs = 0.0
        else:
            self._timestamp = np.arange(len(df))
            self._data = df.values
            self._fs = 1000.0

        return self._timestamp, self._data

    def get_value(self, ch_index):
        """获取通道数据 (0=时间戳, 1+=数据通道)"""
        if self._data is None:
            raise RuntimeError("请先 load_data()")

        if ch_index == 0:
            return self._timestamp.copy()
        arr = self._data[:, ch_index - 1]
        return arr.flatten() if arr.ndim > 1 else arr.copy()

    def add_channel(self, data, name='Channel', color=None):
        """添加绘图通道"""
        arr = np.asarray(data)
        if arr.ndim == 2 and arr.shape[1] == 1:
            arr = arr.flatten()
        if arr.ndim != 1:
            raise ValueError(f"数据必须是一维: {arr.shape}")

        self._channels.append({
            'data': arr,
            'name': name,
            'color': color or DEFAULT_COLORS[len(self._channels) % len(DEFAULT_COLORS)]
        })

    def clear_channels(self):
        """清除绘图通道"""
        self._channels = []

    def print_info(self):
        """打印数据信息"""
        print(f"CSV: {self.csv_filepath}")
        print(f"样本数: {self._data.shape[0]}, 通道数: {self._data.shape[1]}")
        if self.channel0_is_timestamp:
            print(f"采样频率: {self._fs:.1f} Hz")
        if self._channels:
            print(f"绘图: {[c['name'] for c in self._channels]}")

    def plot(self):
        """绘制时域图"""
        app = QtWidgets.QApplication([])
        win = pg.GraphicsLayoutWidget(title="VOFA+ 时域图")
        win.resize(1200, 600)
        plot = win.addPlot()

        x = self._timestamp / 1000 if self.channel0_is_timestamp else self._timestamp
        plot.setLabel('bottom', 'Time' if self.channel0_is_timestamp else 'Index',
                     units='ms' if self.channel0_is_timestamp else None)

        pen_style = _PEN_STYLE_MAP.get(LINE_STYLE.lower(), _PEN_STYLE_MAP['solid'])

        for ch in self._channels:
            color = ch['color']
            qcolor = QtGui.QColor(color) if color.startswith('#') else color
            pen = pg.mkPen(qcolor, width=LINE_WIDTH, style=pen_style)

            brush = None
            fill_level = None
            if FILL_ENABLED:
                fc = QtGui.QColor(color if isinstance(color, str) else qcolor)
                fc.setAlpha(FILL_ALPHA)
                brush = QtGui.QBrush(fc)
                fill_level = 0

            plot.plot(x, ch['data'], pen=pen, name=ch['name'], fillLevel=fill_level, brush=brush)

        plot.addLegend()
        plot.showGrid(x=True, y=True, alpha=0.3)

        # 十字交叉线
        vline = pg.InfiniteLine(angle=90, pen=pg.mkPen('g', width=1, style=pen_style))
        hline = pg.InfiniteLine(angle=0, pen=pg.mkPen('g', width=1, style=pen_style))
        plot.addItem(vline, ignoreBounds=True)
        plot.addItem(hline, ignoreBounds=True)

        def on_mouse(evt):
            pos = evt[0]
            if plot.sceneBoundingRect().contains(pos):
                p = plot.vb.mapSceneToView(pos)
                vline.setPos(p.x())
                hline.setPos(p.y())

        pg.SignalProxy(plot.scene().sigMouseMoved, rateLimit=60, slot=on_mouse)
        win.show()
        app.exec()


def get_analyzer():
    """获取分析器单例"""
    return VofaAnalyzer()