fn main() {
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let root_dir = std::path::Path::new(&manifest_dir)
        .parent()
        .unwrap()
        .parent()
        .unwrap();
    let include_path = root_dir.join("include");
    let src_path = root_dir.join("src");

    let bindings_header = include_path.join("tbmp_bindings.h");
    println!("cargo:rerun-if-changed={}", bindings_header.display());
    println!("cargo:rerun-if-changed={}", src_path.display());

    let mut build = cc::Build::new();
    build.include(include_path.to_str().unwrap());
    build.flag("-Wall");
    build.flag("-Wextra");
    build.flag("-Wno-unused-parameter");
    build.file(src_path.join("tbmp_reader.c"));
    build.file(src_path.join("tbmp_decode.c"));
    build.file(src_path.join("tbmp_pixel.c"));
    build.file(src_path.join("tbmp_msgpack.c"));
    build.file(src_path.join("tbmp_meta.c"));
    build.compile("tbmp");
    println!("cargo:rustc-link-lib=static=tbmp");
    println!(
        "cargo:rustc-link-search=native={}/target/debug/deps",
        manifest_dir
    );

    let builder = bindgen::Builder::default()
        .header(bindings_header.to_str().unwrap())
        .allowlist_type("TBmp.*")
        .allowlist_function("tbmp_.*")
        .allowlist_var("TBMP_.*")
        .generate_inline_functions(true);

    let bindings = builder.generate().expect("Unable to generate bindings");

    let out_dir = std::path::Path::new(&manifest_dir).join("src");
    std::fs::create_dir_all(&out_dir).expect("Failed to create src dir");
    let out_file = out_dir.join("bindings.rs");

    let generated = bindings.to_string();
    let fixed = fix_constant_names(&generated);
    std::fs::write(out_file, fixed).expect("Unable to write bindings");
}

fn fix_constant_names(src: &str) -> String {
    let mut result = src.to_string();
    result = result.replace("TBmpEncoding_TBMP_", "TBMP_ENCODING_TBMP_");
    result = result.replace("TBmpPixelFormat_TBMP_", "TBMP_PIXEL_FORMAT_TBMP_");
    result = result.replace("TBmpError_TBMP_", "TBMP_ERROR_TBMP_");
    result = result.replace("TBmpRotateFilter_TBMP_", "TBMP_ROTATE_FILTER_TBMP_");
    result
}
