// TypeScript bindings for emscripten-generated code.  Automatically generated at compile time.
declare namespace RuntimeExports {
    /**
     * @param {string|null=} returnType
     * @param {Array=} argTypes
     * @param {Array=} args
     * @param {Object=} opts
     */
    function ccall(ident: any, returnType?: (string | null) | undefined, argTypes?: any[] | undefined, args?: any[] | undefined, opts?: any | undefined): any;
    /**
     * @param {string=} returnType
     * @param {Array=} argTypes
     * @param {Object=} opts
     */
    function cwrap(ident: any, returnType?: string | undefined, argTypes?: any[] | undefined, opts?: any | undefined): any;
    /**
     * Given a pointer 'ptr' to a null-terminated UTF8-encoded string in the
     * emscripten HEAP, returns a copy of that string as a Javascript String object.
     *
     * @param {number} ptr
     * @param {number=} maxBytesToRead - An optional length that specifies the
     *   maximum number of bytes to read. You can omit this parameter to scan the
     *   string until the first 0 byte. If maxBytesToRead is passed, and the string
     *   at [ptr, ptr+maxBytesToReadr[ contains a null byte in the middle, then the
     *   string will cut short at that byte index.
     * @param {boolean=} ignoreNul - If true, the function will not stop on a NUL character.
     * @return {string}
     */
    function UTF8ToString(ptr: number, maxBytesToRead?: number | undefined, ignoreNul?: boolean | undefined): string;
    function allocate(slab: any, allocator: any): any;
    let HEAPU8: Uint8Array;
}
interface WasmModule {
  _free(_0: number): void;
  _malloc(_0: number): number;
  _tbmp_reset(): void;
  _tbmp_load(_0: number, _1: number): number;
  _tbmp_width(): number;
  _tbmp_height(): number;
  _tbmp_pixel_format(): number;
  _tbmp_encoding(): number;
  _tbmp_bit_depth(): number;
  _tbmp_error(): number;
  _tbmp_has_palette(): number;
  _tbmp_has_masks(): number;
  _tbmp_has_extra(): number;
  _tbmp_has_meta(): number;
  _tbmp_pixels_len(): number;
  _tbmp_pixels_ptr(): number;
  _tbmp_pixels_copy(_0: number, _1: number): number;
  _tbmp_image_info_json(_0: number, _1: number): number;
  _tbmp_meta_json(_0: number, _1: number): number;
  _tbmp_palette_json(_0: number, _1: number): number;
  _tbmp_masks_json(_0: number, _1: number): number;
  _tbmp_extra_json(_0: number, _1: number): number;
  _tbmp_cleanup(): void;
  _tbmp_error_string(_0: number, _1: number, _2: number): number;
  _tbmp_encode(_0: number, _1: number, _2: number, _3: number, _4: number, _5: number, _6: number, _7: number): number;
  _tbmp_write_needed(_0: number, _1: number, _2: number, _3: number, _4: number): number;
  _tbmp_rotate_90(): number;
  _tbmp_rotate_180(): number;
  _tbmp_rotate_270(): number;
  _tbmp_rotate_output_size(_0: number, _1: number, _2: number, _3: number, _4: number): number;
  _tbmp_rotate_any(_0: number, _1: number, _2: number, _3: number, _4: number, _5: number): number;
  _tbmp_convert_pixel(_0: number, _1: number): number;
  _tbmp_dither(_0: number): number;
  _tbmp_validate(_0: number, _1: number, _2: number, _3: number, _4: number): number;
}

export type MainModule = WasmModule & typeof RuntimeExports;
export default function MainModuleFactory (options?: unknown): Promise<MainModule>;
