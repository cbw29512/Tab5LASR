// LASRJPEGV1 is a checksummed, bounded *real Tab5 camera* JPEG over USB.
// No fallback art, fake frames, or browser camera are used.
export function parseUsbJpegLine(line) {
  try {
    if (typeof line !== "string" || !line.startsWith("LASRJPEGV1 ")) return null;
    const fields = line.trim().split(" ");
    if (fields.length !== 7) return null;
    const [magic, seqText, wText, hText, lenText, hex, base64] = fields;
    if (magic !== "LASRJPEGV1") return null;
    if (!/^\d+$/.test(seqText) || !/^\d+$/.test(wText) ||
        !/^\d+$/.test(hText) || !/^\d+$/.test(lenText) ||
        !/^[0-9a-fA-F]{8}$/.test(hex) || !/^[A-Za-z0-9+/]+={0,2}$/.test(base64)) return null;
    const seq=Number(seqText),width=Number(wText),height=Number(hText),
      size=Number(lenText);
    if (!Number.isSafeInteger(seq) || seq<1 || width!==320 || height!==176 ||
        size<4 || size>65536 || base64.length!==4*Math.ceil(size/3)) return null;
    const raw=atob(base64);
    if (raw.length!==size || raw.charCodeAt(0)!==255 ||
        raw.charCodeAt(1)!==216 || raw.charCodeAt(size-2)!==255 ||
        raw.charCodeAt(size-1)!==217) return null;
    let hash=2166136261;
    for(let i=0;i<raw.length;i++)
      hash=Math.imul(hash ^ raw.charCodeAt(i),16777619)>>>0;
    if (hash!==Number.parseInt(hex,16)) return null;
    return {seq,width,height,size,base64};
  } catch (error) {
    console.warn("Discarded invalid/incomplete USB camera JPEG",error);
    return null;
  }
}
