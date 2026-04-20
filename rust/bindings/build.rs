fn main() {
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    // Try to find headers
    let find_header = || -> Option<std::path::PathBuf> {
        let locations = [
            "../include/tbmp_bindings.h",
            "../include/tbmp.h",
            "include/tbmp_bindings.h",
            "../../include/tbmp_bindings.h",
        ];

        for loc in locations {
            let p = std::path::Path::new(&manifest_dir).join(loc);
            if p.exists() {
                return Some(p);
            }
        }
        None
    };

    let bindings_header = find_header();

    // Generate bindings if header found (dev build)
    let out_file = std::path::Path::new(&manifest_dir).join("src/bindings.rs");

    if let Some(header) = bindings_header {
        println!("cargo:rerun-if-changed={}", header.display());

        let builder = bindgen::Builder::default()
            .header(header.to_str().unwrap())
            .allowlist_type("TBmp.*")
            .allowlist_function("tbmp_.*")
            .allowlist_var("TBMP_.*")
            .generate_inline_functions(true);

        let bindings = builder.generate().expect("Unable to generate bindings");
        let generated = bindings.to_string();
        let fixed = fix_constant_names(&generated);
        std::fs::write(&out_file, fixed).expect("Unable to write bindings");
    } else if !out_file.exists() {
        panic!("tbmp header not found. Expected one of: include/tbmp_bindings.h or include/tbmp.h");
    } else {
        println!("cargo:warning=Header not found, using existing bindings.rs");
        println!("cargo:warning=For dev builds, ensure include/ directory is accessible");
    }
}

fn fix_constant_names(src: &str) -> String {
    let mut result = src.to_string();
    result = result.replace("TBmpEncoding_TBMP_", "TBMP_ENCODING_TBMP_");
    result = result.replace("TBmpPixelFormat_TBMP_", "TBMP_PIXEL_FORMAT_TBMP_");
    result = result.replace("TBmpError_TBMP_", "TBMP_ERROR_TBMP_");
    result = result.replace("TBmpRotateFilter_TBMP_", "TBMP_ROTATE_FILTER_TBMP_");
    result
}