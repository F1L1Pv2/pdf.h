#include "pdf.h"
#include <errno.h>

int main(int argc, char** argv){
    if(argc < 2){ fprintf(stderr,"Provide filename\n"); return 1; }

    size_t ids = 0;
    size_t catalogue = ++ids;
    size_t pages = ++ids;
    size_t resources = ++ids;
    size_t page = ++ids;
    size_t page_content = ++ids;
    size_t font = ++ids;
    size_t font_descriptor = ++ids;
    size_t font_stream = ++ids;
    size_t jimbo = ++ids;
    String_Builder sa = {0};
    PDFObject_Array tmp = {0};
    PDFObject_Array tmp2 = {0};
    PDFObject_Indirects pdf = {0};
    da_push(&pdf, PDFCatalogue(catalogue,pages));
    da_push(&pdf, PDFPages(pages,&tmp,page));
    da_push(&pdf, PDFObject_indirect(
        resources,0,
        PDFObject_dictionary(
            PDF_entry("Font",PDFObject_dictionary(
               PDF_entry("F1", PDFObject_refrence(font,0))
            )),
            PDF_entry("XObject",PDFObject_dictionary(
               PDF_entry("Im1", PDFObject_refrence(jimbo,0))
            ))
        )
    ));
    da_push(&pdf,PDFPage(page, pages, 0, 0, 595.28, 841.89, resources, page_content));
    da_push(&pdf,PDFContentStream(page_content, &sa,
        PDFinstruction(q),
        PDFinstruction(cm,
            PDFObject_number_unsigned(400),
            PDFObject_number_unsigned(0),
            PDFObject_number_unsigned(0),
            PDFObject_number_unsigned(300),
            PDFObject_number_unsigned(100),
            PDFObject_number_unsigned(200)
        ),
        PDFinstruction(Do,PDFObject_name("Im1")),
        PDFinstruction(Q),
    
        PDFinstruction(BT),
        PDFinstruction(Tf,PDFObject_name("F1"),PDFObject_number_unsigned(26)),
        PDFinstruction(Td,PDFObject_number_unsigned(80),PDFObject_number_unsigned(770)),
        PDFinstruction(Tj,PDFObject_string_alloc(&sa,"MAN THIS WORKY!")),
        PDFinstruction(ET),
    
        PDFinstruction(BT),
        PDFinstruction(Tf,PDFObject_name("F1"),PDFObject_number_unsigned(13)),
        PDFinstruction(Td,PDFObject_number_unsigned(50),PDFObject_number_unsigned(550)),
        PDFinstruction(Tj,PDFObject_string_alloc(&sa,"Firsto!")),
        PDFinstruction(ET)
    ));
    da_push(&pdf,PDFJPG(&sa,jimbo,"jimbo.jpg",NULL,NULL));
    if(PDFFont(&pdf, &sa,&tmp,&tmp2, F_REG,font,font_stream, font_descriptor, "font.ttf", "AgravePro") != 0) return 1;
    String_Builder sb = {0};
     Objects objects = {0};
    if(write_pdf(&sb, &objects,pdf.items,pdf.len) != 0) return 1;
 
    FILE* f = fopen(argv[1], "wb");
    if(!f) return 1;
    fwrite(sb.items,1,sb.len,f);
    fclose(f);
    printf("Generated %s!\n", argv[1]);
    return 0;
}
