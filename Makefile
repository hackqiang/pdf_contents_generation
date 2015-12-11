all: pcg

mupdf_path=../mupdf
INCLUDE_PATH=-I$(mupdf_path)/include -I$(mupdf_path)/generated -I$(mupdf_path)/scripts/freetype -I$(mupdf_path)/thirdparty/freetype/include -I$(mupdf_path)/thirdparty/jbig2dec -I$(mupdf_path)/scripts/jpeg -I$(mupdf_path)/thirdparty/jpeg -I$(mupdf_path)/scripts/openjpeg -I$(mupdf_path)/thirdparty/openjpeg/libopenjpeg  -I$(mupdf_path)/thirdparty/zlib -I$(mupdf_path)/thirdparty/mujs
THIRD_LIBS=$(mupdf_path)/build/debug/libfreetype.a $(mupdf_path)/build/debug/libjbig2dec.a $(mupdf_path)/build/debug/libjpeg.a $(mupdf_path)/build/debug/libopenjpeg.a $(mupdf_path)/build/debug/libz.a $(mupdf_path)/build/debug/libmujs.a
SHARE_LIBS=-lm -lcrypto
##-lcrypto need "sudo apt-get install libssl-dev"

mupdf:
	cd $(mupdf_path) && make

pcg: pcg.c mupdf
	cc -Wall -pipe -g -o pcg pcg.c $(mupdf_path)/build/debug/libmupdf.a $(THIRD_LIBS) $(SHARE_LIBS) $(INCLUDE_PATH)

test: pcg
	@for name in `ls contents_samples/*.pdf`; \
    do \
        echo "genetate $$name"; \
        ./pcg $$name > $$name.log 2>&1; \
    done

fontinfo: mupdf
	@for name in `ls contents_samples/*.pdf`; \
    do \
        echo "get font info $$name"; \
        $(mupdf_path)/build/debug/mutool info -F $$name > $$name.fontinfo.txt; \
    done

gs:
	../gs/bin/./gs -dNOSAFER -dNOPAUSE -dNOPROMPT -dBATCH -sDEVICE=pdfwrite -dFirstPage=1 -dLastPage=2 -sOutputFile=contents_samples/x.pdf ../pdf/1.pdf

clean:
	rm -f a.out pcg contents_samples/*.txt contents_samples/*.log