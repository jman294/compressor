#ifndef GENERIC_H_   /* Include guard */
#define GENERIC_H_

char *modelFolderPrefix = "src/include/packingtape/models/";
char *modelHeaderTemplate = "/* Model $1 Data File */\r\n#ifndef $1_H_   /* Include guard */\r\n#define $1_H_\r\n\r\nModelData_t $1_Data = {$2};\r\n\r\n#endif // $1_H_";

char *replaceWord(const char *s, const char *oldW,
                                 const char *newW);

#endif // GENERIC_H_
