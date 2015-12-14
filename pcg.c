/*
 * encoding this file as UTF-8
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mupdf/fitz.h>

#include "pcg.h"

//#define DEBUG

//em... bad idea
static int uni[MAX_KEYWORD_LEN/4] = {0};
static outline_tree outline_root;

//must be utf8
static char content_keywords[KEYWORKS_NUM][MAX_KEYWORD_LEN] = 
{
    "目 录",
    "目  录",
    "目　录",
    "目!!录",
    "目录",
    "content",
    "CONTENT",
    "contents",
    "CONTENTS",
    "ｃｏｎｔｅｎｔ",
    "ＣＯＮＴＥＮＴ",
    "ｃｏｎｔｅｎｔｓ",
    "ＣＯＮＴＥＮＴＳ",
};

static char chapter_keywords[KEYWORKS_NUM][MAX_KEYWORD_LEN] = 
{
    "Chapter",
    "chapter",
    //"第一章", //do not care for now
    //"第1章",
    //"第 1 章",
    "第",
};


int count(int *text, int *match)
{
    int count = 0;
    int text_len = 0;
    int match_len = 0;

    while(*(text+text_len)) text_len++;
    while(*(match+match_len)) match_len++;
    
    //printf("count %d %d\n", text_len, match_len);
    
    int p;
    for(p=0;p<text_len;p++)
    {
        if(text[p] == match[0])
        {
            int m = 1;
            while(m<match_len && text[p+m] == match[m]) m++;
            if(m==match_len)
            {
                count++;
                p+=m;
            }
        }
    }
    return count;
}


int search(int *text, int *match)
{
    int text_len = 0;
    int match_len = 0;

    while(*(text+text_len)) text_len++;
    while(*(match+match_len)) match_len++;
    
    //printf("search %d %d\n", text_len, match_len);
    
    int p;
    for(p=0;p<text_len;p++)
    {
        if(text[p] == match[0])
        {
            int m = 1;
            while(m<match_len && text[p+m] == match[m]) m++;
            if(m==match_len)
            {
                return p;
            }
        }
    }
    return -1;
}


// size < MAX_KEYWORD_LEN
int *tounicode(char *utf8)
{
    int *unicode = uni;
    memset(unicode, 0, MAX_KEYWORD_LEN);
    while (*utf8)
    {
        utf8 += fz_chartorune(unicode++, (char *)utf8);
    }
    return uni;
}

int print_unicode(int *unicode, int len)
{
    int pos;
    
    if(len==0)
        while(*(unicode+len)) len++;
    
    for (pos = 0; pos < len && unicode[pos]; pos++)
    {
        char temp[4] = {0};
        fz_runetochar(temp,unicode[pos]);
        printf("%s", temp);
    }
    
    return 0;
}

//
int write_unicode(int fd, int *unicode)
{
    int pos;
    int len=0;
    while(*(unicode+len)) len++;
    
    //em.. Text String Type need UTF-16BE
    char t = 0xfe;
    write(fd, &t, 1);
    t=0xff;
    write(fd, &t, 1);
    
    for (pos = 0; pos < len; pos++)
    {
        if(unicode[pos]<0x10000)
        {
            t = (unicode[pos] >> 8) & 0xff;
            write(fd, &t, 1);
            t = (unicode[pos]) & 0xff;
            write(fd, &t, 1);
        }
        else
        {
            //not verify yet
            int vh = ((unicode[pos] - 0x10000) & 0xFFC00) >> 10 ;
            int vl =  (unicode[pos] - 0x10000) & 0x3ff;
            short h  =  0xD800 | vh;
            short l =  0xDC00 | vl;
            write(fd, &h, 2); 
            write(fd, &l, 2); 
        }

    }
    
    return 0;
}

char *unicode2utf8(int *unicode, char *utf8, int ulen)
{
    int pos;
    int ui = 0;
    int len=0;
    while(*(unicode+len)) len++;
    
    for (pos = 0; pos < len && unicode[pos]; pos++)
    {
        
        char temp[4] = {0};
        int ret = fz_runetochar(temp,unicode[pos]);
        if(ui+ret>ulen)
        {
            break;
        }
        memcpy(utf8+ui, temp, ret);
        ui+=ret;
    }
    return utf8;
}

int unicode2int(int *unicode)
{
    int num = 0;
    int pos=0;
    while(unicode[pos]>='0' && unicode[pos]<='9')
    {
        num = num*10+(unicode[pos]-'0');
        pos++;
    }
    return num;
}

void add_tree(outline_tree *new_tree)
{
    outline_tree *p = &outline_root;
    while(p->next!=NULL) 
    {
        p = p->next;
    }
    p->next = new_tree;
    outline_root.total++;
}

void print_tree()
{
    outline_tree *p = outline_root.next;
    while(p!=NULL) 
    {
        print_unicode(p->text,0);
        printf("<>");
        printf("%d\n", p->pagen);
        p = p->next;
    }
}

/*分析一条章节信息，应该是类似于：
    第一章　连环奸杀案／003 
    第二章　设下诡局／018
    Chapter 1 .......................................... 1  
    Chapter 2 .......................................... 5
*/
int parse_chapter_info(int *txt, int len)
{
    int pos = 0, i=0;
    
    if(len==0)
        while(*(txt+len)) len++;    
    
    int *new_chapter = (int *)malloc((len+1)*sizeof(int));
    if(!new_chapter)
    {
        fprintf(stderr, "malloc %lu error\n", (len+1)*sizeof(int));
        return -1;
    }
    memset(new_chapter, 0, (len+1)*sizeof(int));
    
    //1. 去掉"." 
    while(txt[pos] && pos<len)
    {
        if(txt[pos]!='.')
        {
            new_chapter[i++] = txt[pos];
        }
        pos++;
    }

    //2. 去掉末尾非数字字符
    int j=i-1;
    while((new_chapter[j]<'0' || new_chapter[j]>'9') && j>=0)
    {
        new_chapter[j] = 0;
        j--;
    }
    
    if(j==-1)
    {
        fprintf(stderr, "no page number found\n");
        return -1;
    }
    
    //3. 检测末尾的数字
    int num = j;
    while(new_chapter[num]>='0' && new_chapter[num]<='9')
    {
        num--;
    }

    outline_tree *new_tree = (outline_tree *)malloc(sizeof(outline_tree));
    if(!new_tree)
    {
        fprintf(stderr, "malloc %lu error\n", (len+1)*sizeof(int));
        free(new_chapter);
        return -1;
    }
    memset(new_tree, 0, sizeof(outline_tree));
    
    //这样，这一段章节信息就可以被分成了两段：
    //part1
    j=0;
    while(j<num+1 && j<31)
    {
        new_tree->text[j] = new_chapter[j];
        j++;
    }
    //print_unicode(new_tree->text,num+1);
    //printf("<->");
    //part2
    new_tree->pagen = unicode2int(new_chapter+num+1);
    //printf("%d\n", new_tree->pagen);

    add_tree(new_tree);
    
    free(new_chapter);
    return 0;
}


int analyse_contents(int *text)
{
    int j = 0;
    while(j<KEYWORKS_NUM && chapter_keywords[j][0])
    {
        printf("searching %s\n", chapter_keywords[j]);
        int pos = 0;
        int pre_pos = -1;
        int *tt = text;
        //根据关键字，得到一行行的章节信息
        while((pos=search(tt, tounicode(chapter_keywords[j])))>=0)
        {
            //printf("get %s %d\n", chapter_keywords[j], pos);
            if(pre_pos>=0)
            {
                //printf("%d\n",pos);
                parse_chapter_info(tt-1,pos);
            }
            tt+=pos;
            tt++;
            pre_pos = pos;
        }
        if(pre_pos>=0)
        {
            //最后一段
            parse_chapter_info(tt-1,0);
            break;
        }
        j++;
    }

    return 0;
}


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
        int *unicode;
        int i=1;
        unicode = tounicode(content_keywords[j]);
        while(unicode[i-1])
        {
            printf("%08x ", unicode[i-1]);
            if(!(i++%16)) printf("\n");
        }
        printf("\n");
        j++;
    }
}

int *new_text_form_page_number(fz_context *ctx, fz_document *doc, FILE *fd, int page)
{
    int pos;
    printf("begin page%d text\n", page);
    
    fz_text_sheet *sheet = fz_new_text_sheet(ctx);
    fz_text_page *text = fz_new_text_page_from_page_number(ctx, doc, page, sheet);
    
    int len = textlen(ctx, text);
    int *tbuf = (int *)malloc(sizeof(int)*(len+1));
    if(!tbuf) {
        fprintf(stderr, "malloc %d error\n", len+1);
        return NULL;
    }
    memset(tbuf,0,sizeof(int)*(len+1));

    for(pos = 0; pos < len; pos++)
    {
        fz_char_and_box cab;
        tbuf[pos] = fz_text_char_at(ctx, &cab, text, pos)->c;
        char ww[4];
        int ret = fz_runetochar(ww,tbuf[pos]);
        fwrite(ww,ret,1,fd);
#ifdef DEBUG
        if(!(pos%16)) printf("\n");
        printf("%08x ", tbuf[pos]);
#endif
    }
#ifdef DEBUG
    printf("\n");
#endif
    return tbuf;
}

void free_text(int *text)
{
    if(text)
        free(text);
}


int test_outline(char *inputfile, int objn, int page_offset)
{
    
	int fd_in = open(inputfile, O_RDONLY);
	if(fd_in<0)
	{
		printf("open %s error\n", inputfile);
		return -1;
	}
    char outputfile[256] = {0};
    strcat(outputfile, inputfile);
    strcat(outputfile, ".out.pdf");
	int fd_out = open(outputfile, O_RDWR | O_CREAT);
	if(fd_out<0)
	{
		printf("open %s error\n", outputfile);
		return -1;
	} 
    
	lseek(fd_in, 0, SEEK_SET);
	char c;
	int flag = 0;
	while(read(fd_in,&c,1)==1)
	{
        //get /Catalog 0x20 [0x0d]
        if(c=='C')
        {
            write(fd_out, &c, 1);
            
            char buf[6] = {0};
            if(read(fd_in, buf, 6)!=6)
            {
                printf("read error\n");
                return -1;
            }
            if(!memcmp(buf,"atalog",6))
            {
                write(fd_out, buf, 6);
                //1
                printf("got Catalog\n");
                char wbuf[32] = {0};
                sprintf(wbuf, "\n/Outlines %d 0 R \n", objn);
                write(fd_out, wbuf, strlen(wbuf));
                write(fd_out, "/PageMode /UseOutlines\n", strlen("/PageMode /UseOutlines\n"));
                flag = 1;
            }
            else
            {
                lseek(fd_in, -6, SEEK_CUR);
            }
        }
        else if (c == 'e' && flag)
        {
            write(fd_out, &c, 1);
            char buf[6] = {0};
            if(read(fd_in, buf, 6)!=6)
            {
                printf("read error\n");
                return -1;
            }

            if(!memcmp(buf,"ndobj",5))
            {
                write(fd_out, buf, 6);
                printf("got endobj\n");
                char wbuf[256] = {0};
                //2
                sprintf(wbuf, "\n%d 0 obj \n", objn);
                write(fd_out, wbuf, strlen(wbuf));


                memset(wbuf,0,256);
                sprintf(wbuf, "<<\n/Count %d \n/First %d 0 R \n/Last %d 0 R\n>>\nendobj \n", outline_root.total, objn+1, objn+outline_root.total);
                write(fd_out, wbuf, strlen(wbuf));

                //3
                //todo: outline_root.total==1
                int index = 1;
                outline_tree *p = outline_root.next;
                while(p!=NULL) 
                {
                    memset(wbuf,0,256);
                    sprintf(wbuf, "%d 0 obj \n", objn+index);
                    write(fd_out, wbuf, strlen(wbuf));
                    
                    write(fd_out, "<<\n/Title (", strlen("<<\n/Title ("));
                    
                    //write unicode of text
                    printf("add ");
                    print_unicode(p->text,0);
                    printf(" %d\n", p->pagen);
                    
                    write_unicode(fd_out, p->text);
                    
                    memset(wbuf,0,256);
                    if(index == 1)
                    {
                        sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n>>\nendobj\n", 
                                        p->pagen + page_offset, objn, objn+index+1);
                    }
                    else if(index == outline_root.total)
                    {
                        sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                        p->pagen + page_offset, objn, objn+index-1);
                    }
                    else
                    {
                        sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                        p->pagen, objn, objn+index+1, objn+index-1);
                    }
                    write(fd_out, wbuf, strlen(wbuf));
                    
                    
                    p = p->next;
                    index++;
                }
                
                flag = 0;
            }
            else
            {
                lseek(fd_in, -6, SEEK_CUR);
            }
        }
        else
        {
            write(fd_out, &c, 1);
        }
	}

	close(fd_in);
	close(fd_out);
    return 0;
}

//todo
int fix_page_offset()
{
    return 0;
}

/*
    After reading the pdf spec, I think for support outlines, below steps are necessary:
    0. get the number of objects in pdf, make sure the new objects' ID are not exist;
    1. add outlines in catalog, such as:
    	/Type /Catalog
        /Outlines 3135 0 R 
        /PageMode /UseOutlines
    2. add outline dictionary, such as:
        3135 0 obj
        <<
            /Count 5 
            /First 3136 0 R 
            /Last 3140 0 R
        >>
        endobj
    3. add each outline object, such as:
        3136 0 obj
        <<
             
            /Title (chapter 1 who are you)
            /Dest [2 /Fit]
            /Parent 3135 0 R 
            /Next 3137 0 R
        >>
        3137 0 obj
        <<
             
            /Title (chapter 2 make friend)
            /Dest [20 /Fit]
            /Parent 3135 0 R 
            /Prev 3136 0 R
            /Next 3138 0 R
        >>
        endobj
        ...
        3140 0 obj
        <<
             
            /Title (chapter 5 last war)
            /Dest [80 /Fit]
            /Parent 3135 0 R 
            /Prev 3139 0 R
        >>
        endobj
    4. fix xref
        I think maybe mupdf can fix automaticly, so don't care now.

 */
int update_outlines(char *inputfile, fz_context *ctx, fz_document *doc)
{
    print_tree();
    
    //0
    int objn = pdf_count_objects(ctx,doc);
    int page_offset = fix_page_offset();
    
    printf("total %d objs, page offset %d\n", objn, page_offset);
    
    test_outline(inputfile, objn, page_offset);

    return 0;
}


int main(int argc, char **argv)
{
	char *inputfile;
	int page_count;
	fz_context *ctx;
	fz_document *doc;
    fz_rect search_hit_bbox[32];
    int *text;

	if (argc < 2)
	{
		fprintf(stderr, "usage: ./cg inputfile_pdf_file\n");
		return EXIT_FAILURE;
	}
	inputfile = argv[1];

    memset(&outline_root, 0, sizeof(outline_tree));
    
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
		doc = fz_open_document(ctx, inputfile);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot open document: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}
    
    fz_outline *outline = fz_load_outline(ctx, doc);
    if(outline)
    {
        fprintf(stderr, "pdf already contains outline, skip!\n");
        fz_drop_outline(ctx, outline);
        return 0;
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

    char outputfile[256] = {0};
    strcat(outputfile, inputfile);
    strcat(outputfile, ".contents.txt");
    FILE *fd = fopen(outputfile,"w");
    if(!fd)
    {
        fprintf(stderr, "open %s error\n", outputfile);
        return -1;
    }

#ifdef DEBUG
    printkeyunicode();
#endif

    int count = 0;
    int current_page;
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
        fprintf(stderr, "%s No contents found!!!\n", inputfile);
        for(current_page=0; current_page<SERACH_MAX_PAGE && current_page<page_count; current_page++)
        {
            fwrite("dump pages\n", strlen("dump pages\n"),1,fd);
            text = new_text_form_page_number(ctx,doc,fd,current_page);
            if(!text)
            {
                fprintf(stderr, "get page%d error\n", current_page);
                fclose(fd);
                fz_drop_document(ctx, doc);
                fz_drop_context(ctx);
                return -1;
            }
            //em.. do nothing
            free_text(text);
        }
    }
    else
    {
        text = new_text_form_page_number(ctx,doc,fd,current_page);
        if(!text)
        {
            fprintf(stderr, "get page%d error\n", current_page);
            fclose(fd);
            fz_drop_document(ctx, doc);
            fz_drop_context(ctx);
            return -1;
        }
        // get contents success, begin to analyse
        analyse_contents(text);
        update_outlines(inputfile, ctx, doc);
        free_text(text);
    }

    fclose(fd);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return 0;
}
