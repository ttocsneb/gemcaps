// This code has been taken from https://github.com/lasselukkari/MimeTypes
//
// I may improve on it someday to dynamically retrieve mimetypes regardless of os, but for now
// hardcoding them in is fine
#ifndef MIMETYPES_H_
#define MIMETYPES_H_
#include <string.h>

class MimeTypes {
  public:
    static const char* getType(const char * path);
    static const char* getExtension(const char * type, int skip = 0);

  private:
    struct entry {
      const char* fileExtension;
      const char* mimeType;
    };
    static MimeTypes::entry types[349];
    static int strcmpi(const char *s1, const char *s2);
};

inline MimeTypes mimeTypes;

#endif