# tbmp

WebAssembly bindings for tBMP - a tiny bitmap format library.

## Install

```bash
npm install tbmp
```

## Usage

```javascript
import TBmp from 'tbmp';

const response = await fetch('image.tbmp');
const buffer = await response.arrayBuffer();
const data = new Uint8Array(buffer);

const len = TBmp.tbmp_load(data, data.length);
if (len < 0) {
  const err = TBmp.tbmp_error();
  // handle error
}

const width = TBmp.tbmp_width();
const height = TBmp.tbmp_height();
const pixelsLen = TBmp.tbmp_pixels_len();
const pixels = new Uint8Array(TBmp._malloc(pixelsLen));
TBmp.tbmp_pixels_copy(pixels, pixelsLen);

// Use pixel data...
// RGBA8888 format: 4 bytes per pixel
```

## API

See the [tBMP API Documentation](https://nellowtcs.me/tBMP/docs/api) for full details.

## Browser

```html
<script type="module">
  import TBmp from 'tbmp/tbmp_wasm.js';
  await TBmp.default();
  // ...
</script>
```

## License

Apache-2.0
