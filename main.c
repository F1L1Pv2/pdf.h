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
    String_Builder* sb;
    size_t offset,len;
} String_View;

String_View sv_allocf(String_Builder* sb, const char* fmt, ...){
    String_View sv = {.sb = sb,.offset = sb->len };
    va_list list;
    va_start(list);
    sb_appendf(sb,fmt,list);
    va_end(list);
    sv.len = sb->len - sv.offset;
    return sv;
}

String_View sv_alloc(String_Builder* sb, const char* str){
    String_View sv = {.sb = sb,.offset = sb->len };
    sb_append(sb,str);
    sv.len = sb->len - sv.offset;
    return sv;
}

int sb_append_sv(String_Builder* sb, String_View sv){
    da_reserve(sb,sv.len);
    memcpy(sb->items+sb->len,sv.sb->items+sv.offset,sv.len);
    sb->len += sv.len;
    return sv.len;
}

const char* str_from_sv(String_View sv){
    return sv.sb->items+sv.offset;
}

bool sv_eq(String_View a, String_View b){
    if(a.len != b.len) return false;
    return memcmp(a.sb->items+a.offset,b.sb->items+b.offset,a.len) == 0;
}

typedef size_t Atom;

typedef struct {
    size_t hash;
    String_View sv;
} AtomEntry;

#define MAX_ATOMS 1024

typedef struct {
    AtomEntry* items;
    size_t len, cap;
    String_Builder sb;
    size_t last_hash;
} AtomTable;

AtomTable atom_table = {0};

unsigned long hash(const unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

Atom atom_from_str(const char *str)
{
    if(!str) return 0;

    String_Builder sb = {0};
    sb.items = (char*)&str;
    String_View temp_sv = {
        .sb = &sb,
    };
    {
        size_t len = strlen(str);
        if(!len) return 0;
        temp_sv.len = len;
    }

    size_t h = hash((const unsigned char *)str);

    for (size_t i = 0; i < atom_table.len; ++i) {
        AtomEntry *entry = &atom_table.items[i];

        if (entry->hash == h && sv_eq(entry->sv, temp_sv)) {
            return i;
        }
    }

    String_View sv = sv_alloc(&atom_table.sb, str);

    da_push(&atom_table, ((AtomEntry) {
        .hash = h,
        .sv = sv,
    }));

    return atom_table.len - 1;
}

String_View sv_from_atom(Atom atom)
{
    assert(atom < atom_table.len);
    return atom_table.items[atom].sv;
}


typedef enum{
    PDF_OBJECT_INVALID = 0,
    PDF_OBJECT_BOOLEAN,
    PDF_OBJECT_NUMBER_FLOAT,
    PDF_OBJECT_NUMBER_SIGNED,
    PDF_OBJECT_NUMBER_UNSIGNED,
    PDF_OBJECT_STRING,
    PDF_OBJECT_NAME,
    PDF_OBJECT_ARRAY,
    PDF_OBJECT_DICTIONARY,
    PDF_OBJECT_STREAM,
    PDF_OBJECT_NULL,
    PDF_OBJECT_REFRENCE,
} PDFObject_type;

const char* PDFObject_type_str[] = {
    [PDF_OBJECT_INVALID] = "PDF_OBJECT_INVALID",
    [PDF_OBJECT_BOOLEAN] = "PDF_OBJECT_BOOLEAN",
    [PDF_OBJECT_NUMBER_FLOAT] = "PDF_OBJECT_NUMBER_FLOAT",
    [PDF_OBJECT_NUMBER_SIGNED] = "PDF_OBJECT_NUMBER_SIGNED",
    [PDF_OBJECT_NUMBER_UNSIGNED] = "PDF_OBJECT_NUMBER_UNSIGNED",
    [PDF_OBJECT_STRING] = "PDF_OBJECT_STRING",
    [PDF_OBJECT_NAME] = "PDF_OBJECT_NAME",
    [PDF_OBJECT_ARRAY] = "PDF_OBJECT_ARRAY",
    [PDF_OBJECT_DICTIONARY] = "PDF_OBJECT_DICTIONARY",
    [PDF_OBJECT_STREAM] = "PDF_OBJECT_STREAM",
    [PDF_OBJECT_NULL] = "PDF_OBJECT_NULL",
    [PDF_OBJECT_REFRENCE] = "PDF_OBJECT_REFRENCE",
};

typedef struct PDFObject PDFObject;

typedef struct{
    PDFObject** items;
    size_t len,cap;
} PDFObject_Array;

typedef struct{
    PDFObject* key; //has to be type `name`
    PDFObject* value;
} PDFObject_Dictionary_Entry;

typedef struct{
    PDFObject_Dictionary_Entry* items;
    size_t len,cap;
} PDFObject_Dictionary;

typedef struct{
    size_t id;
    size_t generation;
    PDFObject* object;
    bool catalogue;
} PDFObject_Indirect;

typedef struct{
    size_t id;
    size_t generation;
} PDFObject_Refrence;

//i should use atom table for strings tbh

struct PDFObject{
    PDFObject_type type;
    union{
        bool boolean;
        float number_float;
        int64_t number_signed;
        uint64_t number_unsigned;
        String_View string;
        Atom name;
        PDFObject_Array array;
        PDFObject_Dictionary dictionary;
        String_View stream;
        //NULL is nothing
        PDFObject_Refrence refrence;
    };
};

PDFObject* PDFObject_init(PDFObject object){
    PDFObject* out = calloc(1,sizeof object);
    if(!out) return NULL;
    *out = object;
    return out;
}

PDFObject* PDFObject_boolean(bool boolean){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_BOOLEAN,
        .boolean = boolean,
    });
}

PDFObject* PDFObject_number_float(float number){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_NUMBER_FLOAT,
        .number_float = number,
    });
}

PDFObject* PDFObject_number_signed(int64_t number){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_NUMBER_SIGNED,
        .number_signed = number,
    });
}

PDFObject* PDFObject_number_unsigned(uint64_t number){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_NUMBER_UNSIGNED,
        .number_unsigned = number,
    });
}

PDFObject* PDFObject_string(String_View sv){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_STRING,
        .string = sv,
    });
}
PDFObject* PDFObject_string_alloc(String_Builder* sb, const char* str){
    String_View sv = sv_alloc(sb,str);
    return PDFObject_string(sv);
}

PDFObject* PDFObject_name(const char* name){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_NAME,
        .name = atom_from_str(name),
    });
}

PDFObject* PDFObject_array(PDFObject** items){ //last has to be NULL
    PDFObject array = {
        .type = PDF_OBJECT_ARRAY,
    };
    while(*items){
        da_push(&array.array,*items);
        items++;
    }
    return PDFObject_init(array);
}

PDFObject* PDFObject_dictionary(PDFObject_Dictionary_Entry* items){ //last has to be NULL
    PDFObject dictionary = {
        .type = PDF_OBJECT_DICTIONARY,
    };
    while(items->key){
        da_push(&dictionary.dictionary,*items);
        items++;
    }
    return PDFObject_init(dictionary);
}

PDFObject* PDFObject_stream(String_View sv){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_STREAM,
        .stream = sv,
    });
}

PDFObject* PDFObject_null(String_View sv){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_NULL,
    });
}

PDFObject_Indirect PDFObject_indirect(size_t id, size_t generation, PDFObject* object){
    return (PDFObject_Indirect){ 
        .id = id,
        .generation = generation,
        .object = object,
    };
}

PDFObject_Indirect PDFObject_indirect_catalogue(size_t id, size_t generation, PDFObject* object){
    return (PDFObject_Indirect){ 
        .id = id,
        .generation = generation,
        .object = object,
        .catalogue = true,
    };
}

PDFObject* PDFObject_refrence(size_t id, size_t generation){
    return PDFObject_init((PDFObject){
        .type = PDF_OBJECT_REFRENCE,
        .refrence.id = id,
        .refrence.generation = generation,
    });
}

PDFObject_Dictionary_Entry PDF_entry(const char* key, PDFObject* value){
    return (PDFObject_Dictionary_Entry){
        .key = PDFObject_name(key),
        .value = value,
    };
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

void write_PDFObject(String_Builder* sb, PDFObject* object){
    if(!object || object->type == PDF_OBJECT_NULL) {
        sb_append(sb, "null");
    }

    switch(object->type){
        case PDF_OBJECT_STREAM: {
            sb_appendf(sb, "<< /Length %zu >>stream\n",object->stream.len);
            sb_append_sv(sb,object->stream);
            sb_append(sb,"\nendstream ");
        } break;
        case PDF_OBJECT_STRING: {
            sb_appendf(sb,"(%.*s)",object->string.len,str_from_sv(object->string));
        } break;
        case PDF_OBJECT_NUMBER_FLOAT: {
            sb_appendf(sb,"%f",object->number_float);
        } break;
        case PDF_OBJECT_NUMBER_SIGNED: {
            sb_appendf(sb,"%lld",object->number_signed);
        } break;
        case PDF_OBJECT_NUMBER_UNSIGNED: {
            sb_appendf(sb,"%llu",object->number_unsigned);
        } break;
        case PDF_OBJECT_ARRAY: {
            sb_append(sb, "[ ");
            for(size_t i = 0; i < object->array.len; i++){
                PDFObject* child = object->array.items[i];
                write_PDFObject(sb,child);
                sb_append(sb, " ");
            }
            sb_append(sb, "]");
        } break;
        case PDF_OBJECT_REFRENCE: {
            sb_appendf(sb,"%zu %zu R",object->refrence.id, object->refrence.generation);
        } break;
        case PDF_OBJECT_NAME: {
            String_View name_sv = sv_from_atom(object->name);
            sb_appendf(sb,"/%.*s",name_sv.len,str_from_sv(name_sv));
        } break;
        case PDF_OBJECT_DICTIONARY: {
            sb_append(sb,"<< ");
            for(size_t i = 0; i < object->dictionary.len; i++){
                PDFObject_Dictionary_Entry* entry = &object->dictionary.items[i];
                //TODO add validation step
                /*
                if(!entry->key || entry->key->type != PDF_OBJECT_NAME){
                    printf("dictionary entry key has to be type name\n");
                    return 1;
                }
                */

                write_PDFObject(sb,entry->key);
                sb_append(sb," ");
                write_PDFObject(sb,entry->value);
                sb_append(sb," ");
            }
            sb_append(sb,">>");
        } break;
        default: {
            printf("jinjer %s not implemented\n", PDFObject_type_str[object->type]);
            assert(false && "Unimplemented");
        }
    }
}


int write_pdf(String_Builder* sb, Objects* objects, PDFObject_Indirect* pdf,size_t pdf_len){
    sb->len = 0;
    objects->len = 0;
    //header
    sb_appendf(sb,"%PDF-1.3\n%\xE0\xE3\xCF\xD3\n");

    size_t catalogue_id = 0;
    size_t catalogue_generation = 0;

    //body
    for(size_t i = 0; i < pdf_len; i++){
        PDFObject_Indirect* object = &pdf[i];
        if(object->catalogue){
            if(catalogue_id != 0 && catalogue_id != object->id){
                printf("cannot have mutiple catalogues only the ones with newer generation apply\n");
            }
            catalogue_id = object->id;
            if(object->generation > catalogue_generation) catalogue_generation = object->generation;
        }
        da_push(objects, ((Object){
            .id = object->id,
            .generation = object->generation,
            .offset = sb->len,
        }));
        sb_appendf(sb, "%zu %zu obj ",object->id,object->generation);
        write_PDFObject(sb,object->object);
        sb_append(sb, " endobj\n");
    }

    if(catalogue_id == 0) {
        printf("No catalogue object defined\n");
        return 1;
    }

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
    sb_appendf(sb, "<< /Size %zu /Root %zu %zu R >>\n",real_len,catalogue_id,catalogue_generation);
    sb_appendf(sb,"startxref\n");
    sb_appendf(sb,"%zu\n",xref_offset);
    sb_appendf(sb,"%%%%EOF\n");
    return 0;
}

typedef struct{
    Atom instruction;
    PDFObject** operands;
} PDFInstruction;

PDFInstruction PDFinstruction(const char* name, PDFObject** operands){
    return (PDFInstruction){
        .instruction = atom_from_str(name),
        .operands = operands,
    };
}

String_View sv_alloc_ContentStream(String_Builder* sb, PDFInstruction* instructions){
    String_View sv = {.sb = sb, .offset = sb->len };
    PDFInstruction* instruction = instructions;
    while(instruction->instruction){
        if(instruction->operands){
            PDFObject** operand = instruction->operands;
            while(*operand){
                write_PDFObject(sb,*operand);
                sb_append(sb," ");
                operand++;
            }
        }
        String_View inst = sv_from_atom(instruction->instruction);
        sb_appendf(sb,"%.*s\n",inst.len,str_from_sv(inst));
        instruction++;
    }
    sv.len = sb->len - sv.offset;
    return sv;
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr,"Provide filename\n");
        return 1;
    }

    size_t ids = 0;
    size_t catalogue = ++ids;
    size_t pages = ++ids;
    size_t font = ++ids;
    size_t page = ++ids;
    size_t page_content = ++ids;
    String_Builder sa = {0};

    PDFObject_Indirect pdf[] = {
        PDFObject_indirect_catalogue(
            catalogue,0,
            PDFObject_dictionary((PDFObject_Dictionary_Entry[]){
                PDF_entry("Type", PDFObject_name("Catalog")),
                PDF_entry("Pages", PDFObject_refrence(pages,0)),
                {0},
            })
        ),
        PDFObject_indirect(
            pages,0,
            PDFObject_dictionary((PDFObject_Dictionary_Entry[]){
                PDF_entry("Type", PDFObject_name("Pages")),
                PDF_entry("Kids", PDFObject_array((PDFObject*[]){
                    PDFObject_refrence(page,0),
                    NULL,
                })),
                PDF_entry("Count", PDFObject_number_unsigned(1)),
                {0},
            })
        ),
        PDFObject_indirect(
            font,0,
            PDFObject_dictionary((PDFObject_Dictionary_Entry[]){
                PDF_entry("Type", PDFObject_name("Font")),
                PDF_entry("Subtype", PDFObject_name("Type1")),
                PDF_entry("BaseFont", PDFObject_name("Helvetica")),
                {0},
            })
        ),
        PDFObject_indirect(
            page,0,
            PDFObject_dictionary((PDFObject_Dictionary_Entry[]){
                PDF_entry("Type", PDFObject_name("Page")),
                PDF_entry("Parent", PDFObject_refrence(pages,0)),
                PDF_entry("MediaBox", PDFObject_array((PDFObject*[]){
                    PDFObject_number_float(0),
                    PDFObject_number_float(0),
                    PDFObject_number_float(595.28),
                    PDFObject_number_float(841.89),
                    NULL,
                })),
                PDF_entry("Resources", PDFObject_dictionary((PDFObject_Dictionary_Entry[]){
                    PDF_entry("Font",PDFObject_dictionary((PDFObject_Dictionary_Entry[]){
                        PDF_entry("F1",PDFObject_refrence(font,0)),
                        {0},
                    })),
                    {0},
                })),
                PDF_entry("Contents", PDFObject_refrence(page_content,0)),
                {0},
            })
        ),
        PDFObject_indirect(
            page_content,0,
            PDFObject_stream(
                sv_alloc_ContentStream(&sa, (PDFInstruction[]){
                    PDFinstruction("BT",NULL),
                    PDFinstruction("Tf",(PDFObject*[]){PDFObject_name("F1"),PDFObject_number_unsigned(26),NULL,}),
                    PDFinstruction("Td",(PDFObject*[]){PDFObject_number_unsigned(80),PDFObject_number_unsigned(770),NULL,}),
                    PDFinstruction("Tj",(PDFObject*[]){PDFObject_string_alloc(&sa,"MAN THIS WORKY!"),NULL,}),
                    PDFinstruction("ET",NULL),

                    PDFinstruction("BT",NULL),
                    PDFinstruction("Tf",(PDFObject*[]){PDFObject_name("F1"),PDFObject_number_unsigned(13),NULL,}),
                    PDFinstruction("Td",(PDFObject*[]){PDFObject_number_unsigned(50),PDFObject_number_unsigned(550),NULL,}),
                    PDFinstruction("Tj",(PDFObject*[]){PDFObject_string_alloc(&sa,"Firsto!"),NULL,}),
                    PDFinstruction("ET",NULL),
                    {0},
                })
            )
        ),
    };

    String_Builder sb = {0};
    Objects objects = {0};
    if(write_pdf(&sb, &objects,pdf,sizeof pdf/sizeof *pdf) != 0) return 1;

    FILE* f = fopen(argv[1], "wb");
    fwrite(sb.items,1,sb.len,f);
    fclose(f);

    printf("Generated %s!\n", argv[1]);
    return 0;
}
