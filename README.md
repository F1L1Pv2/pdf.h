# pdf.h

> **Very much a draft.** Personal work-in-progress, not tailored for professional use. The API will change, error handling is minimal and it is only tested on documents I generate myself.

A small PDF writer in C. You build the PDF out of its real parts (objects, dictionaries, streams, references) and `write_pdf` serialises it. Embeds TrueType fonts and JPEG images; gradients, clipping and everything else are plain PDF operators.

## What's in the box

| File | What it is |
|---|---|
| `pdf.h` | The library |
| `main.c` | Working example: one page with an image and two lines of text. **Start here.** |
| `stb_truetype.h`, `stb_image.h` | Bundled [stb](https://github.com/nothings/stb) libraries (public domain / MIT), used for font metrics and reading JPEG size. Nothing to install. |

## Build and run

```
cc -o main main.c stb_image.c stb_truetype.c -lm
./main out.pdf
```

`main.c` expects `jimbo.jpg` and `font.ttf` next to the binary. Swap in your own.

## Usage in short

1. Give every object an ID (`++ids`). The order you push them in doesn't matter.
2. Push the structure: `PDFCatalogue`, `PDFPages`, `PDFPage`, a resources dictionary.
3. Push content with `PDFContentStream(...)` (PDF operators via `PDFinstruction`), or build the stream yourself with `sb_appendf` for generated layouts.
4. Embed assets: `PDFFont(...)`, `PDFJPG(...)`.
5. `write_pdf(...)`, then `fwrite` the result.

`main.c` shows every step.

## Notes

- Use **static** `.ttf` files. Variable fonts are not supported by `stb_truetype`.
- Text strings are single-byte (WinAnsi). Characters like Polish `ą ę ł` need a `/Differences` remap.
- `Tc` and other text-state settings leak past `ET`. Reset them (`0 Tc`) or wrap text in `q ... Q`.
- Use a separate scratch `PDFObject_Array` for each of: page list, font widths, `/Differences`.

## Limits

JPEG only, single-byte fonts only, no stream compression, no encryption, forms or links. Bad input can crash instead of returning an error.
