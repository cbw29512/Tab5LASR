// Plot actual candidate coordinates only. Markers do not imply target scores.
export function renderCandidates(container, empty, candidates) {
  try {
    container.replaceChildren();
    candidates.forEach((hit, index) => {
      const dot = document.createElement("span");
      dot.className = index === candidates.length - 1 ? "hit latest" : "hit";
      dot.style.left = `${hit.x_norm * 100}%`;
      dot.style.top = `${hit.y_norm * 100}%`;
      dot.title = `UNVERIFIED candidate ${index + 1}: x=${hit.x_norm.toFixed(4)}, y=${hit.y_norm.toFixed(4)}`;
      container.append(dot);
    });
    empty.hidden = candidates.length > 0;
  } catch (error) {
    console.error("Could not plot LASR serial candidates", error);
  }
}
