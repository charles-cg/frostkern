import { useState } from "react";
import { StyleSheet, type LayoutChangeEvent } from "react-native";
import Svg, { Circle, Line, Path } from "react-native-svg";

import { ThemedText } from "@/components/themed-text";
import { ThemedView } from "@/components/themed-view";
import { SENSORS, type SensorId } from "@/constants/sensors";
import { Spacing } from "@/constants/theme";
import { useSettings } from "@/context/settings";
import { useTheme } from "@/hooks/use-theme";

type LineGraphProps = {
  sensor: SensorId;
  data: number[];
  height?: number;
};

// Space inside the drawing so dots on the edges aren't cut off.
const INSET = 8;
const GRID_LINES = 4;
const Y_AXIS_WIDTH = 40;
// Half of the caption line height, so a label is centred on its grid line.
const LABEL_HALF_HEIGHT = 8;

export default function LineGraph({
  sensor,
  data,
  height = 180,
}: LineGraphProps) {
  const theme = useTheme();
  const { t } = useSettings();
  const { unit, color } = SENSORS[sensor];
  const [width, setWidth] = useState(0);

  const onLayout = (event: LayoutChangeEvent) => {
    setWidth(event.nativeEvent.layout.width);
  };

  const min = Math.min(...data);
  const max = Math.max(...data);
  const range = max - min || 1; // avoid dividing by 0 when all values match
  const latest = data[data.length - 1];

  const stepX = data.length > 1 ? (width - INSET * 2) / (data.length - 1) : 0;
  const points = data.map((value, i) => ({
    x: INSET + i * stepX,
    y: INSET + (1 - (value - min) / range) * (height - INSET * 2),
  }));

  // One entry per horizontal grid line: where it sits and which value it marks.
  // i = 0 is the top line (max), the last one is the bottom line (min).
  const gridLines = Array.from({ length: GRID_LINES }, (_, i) => {
    const fraction = i / (GRID_LINES - 1);
    return {
      y: INSET + fraction * (height - INSET * 2),
      value: max - fraction * (max - min),
    };
  });

  const linePath = points
    .map((p, i) => `${i === 0 ? "M" : "L"} ${p.x} ${p.y}`)
    .join(" ");

  // The same line, closed down to the bottom: a soft fill under the curve.
  const bottom = height - INSET;
  const areaPath =
    points.length > 1
      ? `${linePath} L ${points[points.length - 1].x} ${bottom} L ${points[0].x} ${bottom} Z`
      : "";

  return (
    <ThemedView
      type="surface"
      style={[styles.card, { borderLeftColor: theme[color] }]}
    >
      <ThemedView type="surface" style={styles.header}>
        <ThemedText type="subtitle" themeColor="textSecondary">
          {t(sensor)}
        </ThemedText>
        <ThemedText type="subtitle">
          {latest?.toFixed(1) ?? "--"}
          <ThemedText type="small" themeColor="textSecondary">
            {" "}
            {unit}
          </ThemedText>
        </ThemedText>
      </ThemedView>

      <ThemedView type="surface" style={styles.plotRow}>
        <ThemedView type="surface" style={[styles.yAxis, { height }]}>
          {gridLines.map(({ y, value }) => (
            <ThemedText
              key={y}
              type="caption"
              themeColor="textSecondary"
              style={[styles.yLabel, { top: y - LABEL_HALF_HEIGHT }]}
            >
              {value.toFixed(1)}
            </ThemedText>
          ))}
        </ThemedView>

        <ThemedView
          type="surface"
          style={[styles.plot, { height }]}
          onLayout={onLayout}
        >
          {width > 0 && data.length > 0 ? (
            <Svg width={width} height={height}>
              {gridLines.map(({ y }) => {
                return (
                  <Line
                    key={y}
                    x1={0}
                    x2={width}
                    y1={y}
                    y2={y}
                    stroke={theme.textSecondary}
                    strokeOpacity={0.2}
                    strokeWidth={1}
                  />
                );
              })}

              <Path d={areaPath} fill={theme[color]} fillOpacity={0.12} />

              <Path
                d={linePath}
                stroke={theme[color]}
                strokeWidth={2.5}
                strokeLinejoin="round"
                strokeLinecap="round"
                fill="none"
              />

              {points.map((p, i) => (
                <Circle
                  key={i}
                  cx={p.x}
                  cy={p.y}
                  r={i === points.length - 1 ? 5 : 3}
                  fill={theme[color]}
                />
              ))}
            </Svg>
          ) : null}
        </ThemedView>
      </ThemedView>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  card: {
    borderRadius: 12,
    padding: Spacing.three,
    gap: Spacing.two,
    alignSelf: "stretch",
    marginHorizontal: 10,
    borderLeftWidth: 4,
  },
  header: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "baseline",
  },
  plotRow: {
    flexDirection: "row",
    gap: Spacing.two,
  },
  yAxis: {
    width: Y_AXIS_WIDTH,
  },
  yLabel: {
    position: "absolute",
    right: 0,
    textAlign: "right",
  },
  plot: {
    flex: 1,
  },
});
