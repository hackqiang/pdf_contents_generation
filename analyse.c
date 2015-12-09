#include <stdio.h>
#include <string.h>
//input file encoding as UTF8

#define is_num(c) (c>='0' && c<='9')

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: ./analyse input_contents_file\n");
		return -1;
	}
	FILE *fin = fopen(argv[1],"r");
    if(!fin)
    {
		fprintf(stderr, "open %s error\n", argv[1]);
		return -1;
    }
    
    char out[128] = {0};
    strcat(out, argv[1]);
    strcat(out, ".out.txt");
	FILE *fout = fopen(out,"w");
    if(!fout)
    {
		fprintf(stderr, "open %s error\n", out);
		return -1;
    }
    
    //1. 在每串数字后加断行
    char tmp;
    int num_flag = 0;
    while(fread(&tmp,1,1,fin))
    {
        fwrite(&tmp,1,1,fout);
        if(is_num(tmp))
        {
            num_flag=1;
        }
        else
        {
            if(num_flag && tmp !='.')
                fwrite("\n",1,1,fout);
            num_flag=0;
        }
    }
    
    return 0;
}