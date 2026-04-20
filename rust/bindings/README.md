# tbmp - Rust bindings for tBMP

Automatic Rust bindings for the [tBMP](https://github.com/NellowTCS/tBMP) tiny bitmap format library.

## Usage

Add to your `Cargo.toml`:

```toml
[dependencies]
tbmp = "0.1"
```

Or use the latest from git:

```toml
[dependencies]
tbmp = { git = "https://github.com/NellowTCS/tBMP", branch = "main" }
```

## Example

```rust
use tbmp::{TBmpImage, TBmpFrame, TBmpRGBA};

fn main() {
    let data = std::fs::read("image.tbmp").expect("Failed to read file");

    let mut img = TBmpImage::default();
    let err = tbmp::tbmp_open(data.as_ptr(), data.len(), &mut img);
    if err != 0 {
        panic!("Failed to open: {}", err);
    }

    let mut pixels = vec![TBmpRGBA::default(); (img.head.width as usize) * (img.head.height as usize)];
    let mut frame = TBmpFrame {
        width: img.head.width,
        height: img.head.height,
        pixels: pixels.as_mut_ptr(),
    };

    let err = tbmp::tbmp_decode(&img, &mut frame);
    if err != 0 {
        panic!("Failed to decode: {}", err);
    }

    println!("Loaded {}x{}", frame.width, frame.height);
}
```

## Building

Requires the tBMP C library headers. Set `TBMP_INCLUDE` environment variable if not at default path:

```bash
export TBMP_INCLUDE=/path/to/include
cargo build
```

## License

Apache-2.0
