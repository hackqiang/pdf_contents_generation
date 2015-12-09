#include <stdio.h>

#include <mupdf/fitz.h>


#define DEBUG

#define SERACH_MAX_PAGE 20

#define KEYWORKS_NUM 128

//must be utf8
static char content_keywords[KEYWORKS_NUM][32] = 
{
    "目 录",
    "目  录",
    "目　录",
    "目!!录",
    "目录",
    "content",
    "contents",
    "ｃｏｎｔｅｎｔｓ",
};

static char chapter_keywords[KEYWORKS_NUM][32] = 
{
    "Chapter%d",
    "chapter%d",
    "Chapter %d",
    "chapter %d",
    "第%d章",
    "第 %d 章",
};

//get form mupdf code
static int textlen(fz_context *ctx, fz_text_page *page)
{
	int len = 0;
	int block_num;

	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;
		fz_text_line *line;
		fz_text_span *span;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->first_span; span; span = span->next)
			{
				len += span->len;
			}
			len++; /* pseudo-newline */
		}
	}
	return len;
}


void printkeyunicode()
{
    int j = 0;
    while(j<KEYWORKS_NUM && content_keywords[j][0])
    {
        printf("unicode of (%s)\n", content_keywords[j]);
        char *s = content_keywords[j];
        int c;
        int i=1;
        while (*s)
        {
            s += fz_chartorune(&c, (char *)s);
            printf("%04x ", c);
            if(!(i++%16)) printf("\n");
        }
        printf("\n");
        j++;
    }
}

int dump_text_form_page_number(fz_context *ctx, fz_document *doc, FILE *fd, int page)
{
    int pos;
    printf("get page%d text\n", page);
    fz_text_sheet *sheet = fz_new_text_sheet(ctx);
    fz_text_page *text = fz_new_text_page_from_page_number(ctx, doc, page, sheet);
    
    int len = textlen(ctx, text);
    int *tbuf = (int *)malloc(sizeof(int)*(len+1));
    if(!tbuf) {
        fprintf(stderr, "malloc %d error\n", len+1);
        return -1;
    }
    memset(tbuf,0,sizeof(int)*(len+1));

    for (pos = 0; pos < len; pos++)
    {
        fz_char_and_box cab;
        tbuf[pos] = fz_text_char_at(ctx, &cab, text, pos)->c;
        char ww[4];
        int ret = fz_runetochar(ww,tbuf[pos]);
        fwrite(ww,ret,1,fd);
#ifdef DEBUG
        if(!(pos%15)) printf("\n");
        printf("%04x ", tbuf[pos]);
#endif
    }
#ifdef DEBUG
    printf("\n");
#endif
    free(tbuf);
    return 0;
}


int main(int argc, char **argv)
{
	char *input;
	int page_count;
	fz_context *ctx;
	fz_document *doc;
    fz_rect search_hit_bbox[32];

	if (argc < 2)
	{
		fprintf(stderr, "usage: ./cg input_pdf_file\n");
		return EXIT_FAILURE;
	}
	input = argv[1];

	/* Create a context to hold the exception stack and various caches. */
	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return EXIT_FAILURE;
	}

	/* Register the default file types to handle. */
	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	/* Open the document. */
	fz_try(ctx)
		doc = fz_open_document(ctx, input);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot open document: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	/* Count the number of pages. */
	fz_try(ctx)
		page_count = fz_count_pages(ctx, doc);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot count number of pages: %s\n", fz_caught_message(ctx));
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

    char output[128] = {0};
    strcat(output, input);
    strcat(output, ".contents.txt");
    FILE *fd = fopen(output,"w");
    if(!fd)
    {
        fprintf(stderr, "open %s error\n", output);
        return -1;
    }

    int count = 0;
    int current_page;
#ifdef DEBUG
    printkeyunicode();
#endif


    int j = 0;
    while(j<KEYWORKS_NUM && content_keywords[j][0] && !count)
    {
        
        for(current_page=0; current_page<SERACH_MAX_PAGE && current_page<page_count; current_page++)
        {
            //printf("serach page%d (%s)\n", current_page, content_keywords[j]);
            /*
             * 有时候，字符虽然是存在pdf中的，用reader也能看到，编码也是OK的，但是用这个API搜索不到，
             *  debug发现，字符串确实是找到了，但是charbox为空，导致mupdf认为找不到(1c2)... 先不care了...
             */
            count = fz_search_page_number(ctx, doc, current_page, content_keywords[j], search_hit_bbox, 32);
            if(count)
            {
                printf("found page%d (%s) %d times\n", current_page,content_keywords[j],count);
                break;
            }
        }
        j++;
    }

    if(!count)
    {
        //for debug, dump first pages
        fprintf(stderr, "%s No contents found!!!\n", input);
        for(current_page=0; current_page<SERACH_MAX_PAGE && current_page<page_count; current_page++)
        {
            fwrite("dump pages\n", strlen("dump pages\n"),1,fd);
            dump_text_form_page_number(ctx,doc,fd,current_page);
        }
    }
    else
    {
        dump_text_form_page_number(ctx,doc,fd,current_page);
    }

    fclose(fd);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return EXIT_SUCCESS;
}
