export async function listExampleFiles() {
  return [
    "all.tbmp",
    "blackAndWhiteP1File.tbmp",
    "colorP3File.tbmp",
    "colorP3FileOnePixelPerLine.tbmp",
    "colorP6File.tbmp",
    "masks.tbmp",
    "metadata.tbmp",
    "palette.tbmp",
    "picsum_block_mode_block_sparse.tbmp",
    "picsum_custom_mode_custom.tbmp",
    "picsum_extra_a_mode_rle.tbmp",
    "picsum_extra_b_mode_span.tbmp",
    "picsum_raw_mode_raw.tbmp",
    "picsum_rle_mode_rle.tbmp",
    "picsum_span_mode_span.tbmp",
    "picsum_sparse_mode_sparse.tbmp",
    "picsum_zerorange_mode_zerorange.tbmp",
    "sample_640x426.tbmp",
  ];
}

export async function fetchExampleFile(filename) {
  // Use absolute path for Vite public assets
  const url = `/tBMP/examples/${encodeURIComponent(filename)}`;
  const res = await fetch(url);
  if (!res.ok) throw new Error(`Failed to fetch example: ${filename}`);
  return await res.blob();
}
