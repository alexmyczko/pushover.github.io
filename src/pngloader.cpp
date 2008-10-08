#include "pngloader.h"


pngLoader_c::pngLoader_c(std::string fname) {

  f = fopen(fname.c_str(), "rb");

  if (!f)
    return;

  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;

  // TODO enter error handler in here
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    return;

  info_ptr = png_create_info_struct(png_ptr);

  if (info_ptr == 0)
  {
    png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
    png_ptr = 0;
    return;
  }

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
    png_ptr = 0;
    return;
  }

  png_init_io(png_ptr, f);

  /* Read PNG header info */
  png_read_info(png_ptr, info_ptr);

  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
      &color_type, &interlace_type, NULL, NULL);

  if (color_type != PNG_COLOR_TYPE_RGB_ALPHA || interlace_type != PNG_INTERLACE_NONE || bit_depth != 8) {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
    png_ptr = 0;
    return;
  }
}

pngLoader_c::~pngLoader_c(void) {

  if ( png_ptr )
    png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : (png_infopp)0, (png_infopp)0);

  if (f)
    fclose(f);
}

unsigned int pngLoader_c::getWidth(void) {
  return png_get_image_width(png_ptr, info_ptr);
}

void pngLoader_c::getPart(SDL_Surface * v) {

  if (!png_ptr) return;

  if (setjmp(png_jmpbuf(png_ptr)))
    return;

  for (int i = 0; i < v->h; i++)
    png_read_row(png_ptr, ((png_byte*)v->pixels) + i*v->pitch, 0);
}

void pngLoader_c::skipLines(unsigned int lines) {
  if (!png_ptr) return;

  if (setjmp(png_jmpbuf(png_ptr)))
    return;

  png_byte * row = new png_byte[png_get_rowbytes(png_ptr, info_ptr)];

  for (unsigned int i = 0; i < lines; i++)
    png_read_row(png_ptr, row, 0);

  delete [] row;
}

