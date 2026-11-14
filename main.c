#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include "darray.h"
#include <stdarg.h>

typedef struct{
    char* items;
    int len,cap;
} String_Builder;

int sb_appendf(String_Builder* sb, const char* fmt, ...){
    va_list list;
    va_start(list);
    int len = vsnprintf(NULL,0,fmt,list);
    va_end(list);
    if(len <= 0) return 0;
    da_reserve(sb,len+1);
    va_start(list);
    vsprintf(sb->items+sb->len,fmt,list);
    va_end(list);
    sb->len += len;
    return len;
}

int sb_append(String_Builder* sb, const char* str){
    int len = strlen(str);
    if(len <= 0) return 0;
    da_reserve(sb,len);
    memcpy(sb->items+sb->len,str,len);
    sb->len += len;
    return len;
}

int sb_append_sb(String_Builder* a, String_Builder* b){
    if(b->len <= 0) return 0;
    da_reserve(a,b->len);
    memcpy(a->items+a->len,b->items,b->len);
    a->len += b->len;
    return b->len;
}

typedef struct{
    size_t id;
    size_t generation;
    size_t offset;
    bool not_present;
} Object;

typedef struct{
    Object* items;
    size_t len,cap;
    size_t last_id;
} Objects;

int Object_sort(const void* ptr_a, const void* ptr_b){
    const Object* a = ptr_a;
    const Object* b = ptr_b;
    return (int)a->id - (int)b->id;
}

void Objects_qsort(Objects* objects){
    qsort(objects->items,objects->len,sizeof *objects->items, Object_sort);
}

size_t Objects_get_new_id(Objects* objects){
    return ++objects->last_id;
}

void sb_append_object(String_Builder* sb, Objects* objects, size_t id, size_t generation, String_Builder* contents){
    da_push(objects, ((Object){
        .id = id,
        .generation = generation,
        .offset = sb->len,
    }));
    sb_appendf(sb, "%zu %zu obj\n",id,generation);
    sb_append_sb(sb,contents);
    sb_append(sb, "endobj\n");
}

void sb_append_stream(String_Builder* sb, String_Builder* contents){
    sb_appendf(sb, "<< /Length %d >>\n",contents->len);
    sb_appendf(sb, "stream\n");
    sb_append_sb(sb,contents);
    sb_appendf(sb, "endstream\n");
}

int write_pdf(String_Builder* sb, Objects* objects){
    //header
    sb_appendf(sb,"%PDF-1.0\n%\xE0\xE3\xCF\xD3\n");

    String_Builder sb_2 = {0};
    String_Builder sb_3 = {0};
    String_Builder* temp = &sb_2;
    String_Builder* temp2 = &sb_3;

    //body
    size_t catalogue = Objects_get_new_id(objects);
    size_t pages = Objects_get_new_id(objects);
    size_t font_resource = Objects_get_new_id(objects);
    size_t page = Objects_get_new_id(objects);
    size_t page_content = Objects_get_new_id(objects);

    //catalog
    temp->len = 0;
    sb_appendf(temp, "<< /Type /Catalog /Pages %zu 0 R >>\n",pages);
    sb_append_object(sb,objects,catalogue,0,temp);

    temp->len = 0;
    sb_appendf(temp, "<< /Type /Pages /Kids [%zu 0 R] /Count 1 >>\n",page);
    sb_append_object(sb,objects,pages,0,temp);

    temp->len = 0;
    sb_appendf(temp, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n");
    sb_append_object(sb,objects,font_resource,0,temp);

    temp->len = 0;
    sb_appendf(temp, "<< /Type /Page /Parent %zu 0 R /MediaBox [0 0 595.28 841.89] /Resources << /Font << /F1 %zu 0 R >> >> /Contents %zu 0 R >>\n",pages,font_resource,page_content);
    sb_append_object(sb,objects,page,0,temp);

    temp->len = 0;
    temp2->len = 0;
    sb_appendf(temp2, "BT\n");
    sb_appendf(temp2, "/F1 26 Tf\n");
    sb_appendf(temp2, "72 770 Td\n");
    sb_appendf(temp2, "(This shit actually works!) Tj\n");
    sb_appendf(temp2, "ET\n");

    sb_appendf(temp2, "BT\n");
    sb_appendf(temp2, "/F1 26 Tf\n");
    sb_appendf(temp2, "72 550 Td\n");
    sb_appendf(temp2, "(SECOND!) Tj\n");
    sb_appendf(temp2, "ET\n");
    sb_append_stream(temp,temp2);
    sb_append_object(sb,objects,page_content,0,temp);

    //xref
    size_t xref_offset = sb->len;
    Objects_qsort(objects);
    size_t real_len = objects->items[objects->len-1].id;
    sb_appendf(sb, "xref\n");
    sb_appendf(sb,"0 %zu\n",real_len);
    sb_appendf(sb,"0000000000 65535 f\n");
    Object* prev = NULL;
    for(size_t i = 0; i < objects->len; i++){
        Object* object = &objects->items[i];
        
        //filling in missing thingies
        if(prev && (object->id - prev->id) > 1){
            for(size_t j = 0; j < object->id - prev->id - 1; j++){
                sb_appendf(sb,"0000000000 65535 f\n");
            }
        }

        sb_appendf(sb,"%010zu %05zu %c\n",object->offset,object->generation, object->not_present ? 'f' : 'n');
        prev = object;
    }

    //trailer
    sb_appendf(sb,"trailer\n");
    sb_appendf(sb, "<< /Size %zu /Root %zu %zu R >>\n",real_len,catalogue,0);
    sb_appendf(sb,"startxref\n");
    sb_appendf(sb,"%zu\n",xref_offset);
    sb_appendf(sb,"%%%%EOF\n");
    return 0;
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr,"Provide filename");
        return 1;
    }

    String_Builder sb = {0};
    Objects objects = {0};
    if(write_pdf(&sb, &objects) != 0) return 1;

    FILE* f = fopen(argv[1], "wb");
    fwrite(sb.items,1,sb.len,f);
    fclose(f);

    printf("Generated %s!\n", argv[1]);
    return 0;
}
