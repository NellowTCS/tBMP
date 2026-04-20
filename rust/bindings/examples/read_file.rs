//! tBMP Rust example - read a tBMP file

use std::fs;
use std::ptr;
use tbmp::{TBmpFrame, TBmpImage, TBmpRGBA};

fn main() {
    let args: Vec<_> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <file.tbmp>", args[0]);
        std::process::exit(1);
    }

    let filename = &args[1];
    let data = fs::read(filename).expect("Failed to read file");

    // Parse the image
    let mut img = unsafe {
        TBmpImage {
            head: std::mem::zeroed(),
            data: ptr::null(),
            data_len: 0,
            has_palette: 0,
            palette: std::mem::zeroed(),
            has_masks: 0,
            masks: std::mem::zeroed(),
            extra: ptr::null(),
            extra_len: 0,
            meta: ptr::null(),
            meta_len: 0,
        }
    };

    unsafe {
        let err = tbmp::tbmp_open(data.as_ptr(), data.len(), &mut img);
        if err != 0 {
            eprintln!("Failed to open: error {}", err);
            std::process::exit(1);
        }

        // Allocate frame
        let required = (img.head.width as usize) * (img.head.height as usize);
        let mut pixels = vec![
            TBmpRGBA {
                r: 0,
                g: 0,
                b: 0,
                a: 0
            };
            required
        ];

        let mut frame = TBmpFrame {
            width: img.head.width,
            height: img.head.height,
            pixels: pixels.as_mut_ptr(),
        };

        let err = tbmp::tbmp_decode(&img, &mut frame);
        if err != 0 {
            eprintln!("Failed to decode: error {}", err);
            std::process::exit(1);
        }

        println!(
            "Loaded: {}x{} {}bpp",
            frame.width, frame.height, img.head.bit_depth
        );

        // Print first pixel
        println!(
            "First pixel: R={} G={} B={} A={}",
            pixels[0].r, pixels[0].g, pixels[0].b, pixels[0].a
        );
    }
}
