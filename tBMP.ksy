meta:
  id: tbmp
  title: Tiny Bitmap Format
  endian: le
  file-extension: tbmp
  ks-version: 0.9

seq:
  - id: magic
    contents: "tBMP"

  - id: head
    type: head

  - id: data
    size: head.data_size
    type:
      switch-on: head.encoding
      cases:
        0: raw
        1: zero_range
        2: rle_rgba
        3: span_rgba
        4: sparse_rgba
        5: block_sparse
        _: raw

  - id: extra
    size: head.extra_size
    if: head.extra_size > 0
    type: extra_chunk

  - id: meta
    size: head.meta_size
    if: head.meta_size > 0
    # raw MessagePack blob

types:
  head:
    seq:
      - id: version
        type: u2
      - id: width
        type: u2
      - id: height
        type: u2

      - id: bit_depth
        type: u1
      - id: encoding
        type: u1
      - id: pixel_format
        type: u1
        enum: pixel_format
      - id: flags
        type: u1
        # bit0 = has_palette, bit1 = has_masks

      - id: data_size
        type: u4
      - id: extra_size
        type: u4
      - id: meta_size
        type: u4
      - id: reserved
        type: u4

  # EXTRA container
  extra_chunk:
    seq:
      - id: entries
        type: extra_entry
        repeat: eos

  extra_entry:
    seq:
      - id: tag
        type: str
        size: 4
        encoding: ASCII
      - id: len
        type: u4
      - id: body
        size: len
        type:
          switch-on: tag
          cases:
            '"PALT"': palette_chunk
            '"MASK"': masks_chunk
            _: raw_bytes

  palette_chunk:
    seq:
      - id: num_entries
        type: u4
      - id: entries
        type: palette_entry
        repeat: expr
        repeat-expr: num_entries

  palette_entry:
    seq:
      - id: r
        type: u1
      - id: g
        type: u1
      - id: b
        type: u1
      - id: a
        type: u1

  masks_chunk:
    seq:
      - id: mask_r
        type: u4
      - id: mask_g
        type: u4
      - id: mask_b
        type: u4
      - id: mask_a
        type: u4

  raw_bytes:
    seq:
      - id: data
        type: u1
        repeat: eos

  # Mode 0: RAW
  raw:
    seq:
      - id: pixels
        type: raw_bytes

  # Mode 1: Zero-Range
  zero_range:
    seq:
      - id: num_zero_ranges
        type: u4

      - id: zero_ranges
        type: zero_range_entry
        repeat: expr
        repeat-expr: num_zero_ranges

      - id: num_values
        type: u4

      - id: values
        type: u1
        repeat: expr
        repeat-expr: num_values
        # note: only valid for bit_depth <= 8; wider depths not supported in zero-range

  zero_range_entry:
    seq:
      - id: start
        type: u4
      - id: length
        type: u4

  # Mode 2: RLE - uses variable-width values
  rle_rgba:
    seq:
      - id: entries
        type: rle_entry_v
        repeat: eos

  rle_entry_v:
    seq:
      - id: count
        type: u1
      - id: b1
        type: u1
      - id: b2
        type: u1
        if: _root.head.bit_depth >= 16
      - id: b3
        type: u1
        if: _root.head.bit_depth >= 24
      - id: b4
        type: u1
        if: _root.head.bit_depth >= 32

  # Mode 3: Span Encoding - uses variable-width values
  span_rgba:
    seq:
      - id: num_spans
        type: u4

      - id: spans
        type: span_entry_v
        repeat: expr
        repeat-expr: num_spans

  span_entry_v:
    seq:
      - id: index
        type: u4
      - id: length
        type: u4
      - id: b1
        type: u1
      - id: b2
        type: u1
        if: _root.head.bit_depth >= 16
      - id: b3
        type: u1
        if: _root.head.bit_depth >= 24
      - id: b4
        type: u1
        if: _root.head.bit_depth >= 32

  # Mode 4: Sparse Pixel List - uses variable-width values
  sparse_rgba:
    seq:
      - id: num_pixels
        type: u4

      - id: pixels
        type: sparse_entry_v
        repeat: expr
        repeat-expr: num_pixels

  sparse_entry_v:
    seq:
      - id: x
        type: u2
      - id: y
        type: u2
      - id: b1
        type: u1
      - id: b2
        type: u1
        if: _root.head.bit_depth >= 16
      - id: b3
        type: u1
        if: _root.head.bit_depth >= 24
      - id: b4
        type: u1
        if: _root.head.bit_depth >= 32

  # Mode 5: Block-Sparse
  block_sparse:
    seq:
      - id: block_width
        type: u2
      - id: block_height
        type: u2
      - id: num_blocks
        type: u4

      - id: blocks
        type: block_entry
        repeat: expr
        repeat-expr: num_blocks

  block_entry:
    seq:
      - id: block_index
        type: u4
      - id: len_block_data
        type: u4
      - id: block_data
        size: len_block_data

enums:
  pixel_format:
    0: index_1
    1: index_2
    2: index_4
    3: index_8
    4: rgb_565
    5: rgb_555
    6: rgb_444
    7: rgb_332
    8: rgb_888
    9: rgba_8888
    10: custom_masked
