/*
  Copyright (C) 2007 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  There are special exceptions to the terms and conditions of the GPL
  as it is applied to this software. View the full text of the exception
  in file LICENSE.exceptions in the top-level directory of this software
  distribution.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/**
  @file  stringutil.c
  @brief String utility functions, mostly focused on SQLWCHAR and charset
         manipulations.
*/


#include "stringutil.h"


CHARSET_INFO *utf8_charset_info= NULL;


/**
  Duplicate a SQLCHAR in the specified character set as a SQLWCHAR.

  @param[in]      charset_info  Character set to convert into
  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in bytes) or
                                destination string (in chars)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Pointer to a newly allocated SQLWCHAR, or @c NULL
*/
SQLWCHAR *sqlchar_as_sqlwchar(CHARSET_INFO *charset_info, SQLCHAR *str,
                              SQLINTEGER *len, uint *errors)
{
  SQLCHAR *pos, *str_end;
  SQLWCHAR *out;
  SQLINTEGER i, out_bytes;
  my_bool free_str= FALSE;

  if (*len == SQL_NTS)
    *len= strlen((char *)str);

  if (!str || *len == 0)
  {
    *len= 0;
    return NULL;
  }

  if (!is_utf8_charset(charset_info->number))
  {
    uint32 used_bytes, used_chars;
    size_t u8_max= (*len / charset_info->mbminlen *
                    utf8_charset_info->mbmaxlen + 1);
    SQLCHAR *u8= (SQLCHAR *)my_malloc(u8_max, MYF(0));

    if (!u8)
    {
      *len= -1;
      return NULL;
    }

    *len= copy_and_convert((char *)u8, u8_max, utf8_charset_info,
                           (char *)str, *len, charset_info,
                           &used_bytes, &used_chars, errors);
    str= u8;
    free_str= TRUE;
  }

  str_end= str + *len;

  out_bytes= (*len + 1) * sizeof(SQLWCHAR);

  out= (SQLWCHAR *)my_malloc(out_bytes, MYF(0));
  if (!out)
  {
    *len= -1;
    return NULL;
  }

  for (pos= str, i= 0; *pos && pos < str_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      pos+= utf8toutf32(pos, (UTF32 *)(out + i++));
    }
    else
    {
      UTF32 u32;
      pos+= utf8toutf32(pos, &u32);
      i+= utf32toutf16(u32, (UTF16 *)(out + i));
    }
  }

  *len= i;
  out[i]= 0;

  if (free_str)
    x_free(str);

  return out;
}


/**
  Duplicate a SQLWCHAR as a SQLCHAR in the specified character set.

  @param[in]      charset_info  Character set to convert into
  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in chars) or
                                destination string (in bytes)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Pointer to a newly allocated SQLCHAR, or @c NULL
*/
SQLCHAR *sqlwchar_as_sqlchar(CHARSET_INFO *charset_info, SQLWCHAR *str,
                             SQLINTEGER *len, uint *errors)
{
  SQLWCHAR *str_end;
  SQLCHAR *out;
  SQLINTEGER i, u8_len, out_bytes;
  UTF8 u8[MAX_BYTES_PER_UTF8_CP + 1];
  uint32 used_bytes, used_chars;

  *errors= 0;

  if (is_utf8_charset(charset_info->number))
    return sqlwchar_as_utf8(str, len);

  if (*len == SQL_NTS)
    *len= sqlwcharlen(str);
  if (!str || *len == 0)
  {
    *len= 0;
    return NULL;
  }

  out_bytes= *len * charset_info->mbmaxlen * sizeof(SQLCHAR) + 1;
  out= (SQLCHAR *)my_malloc(out_bytes, MYF(0));
  if (!out)
  {
    *len= -1;
    return NULL;
  }

  str_end= str + *len;

  for (i= 0; str < str_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      u8_len= utf32toutf8((UTF32)*str++, u8);
    }
    else
    {
      UTF32 u32;
      str+= utf16toutf32((UTF16 *)str, &u32);
      u8_len= utf32toutf8(u32, u8);
    }

    i+= copy_and_convert((char *)out + i, out_bytes - i, charset_info,
                         (char *)u8, u8_len, utf8_charset_info, &used_bytes,
                         &used_chars, errors);
  }

  *len= i;
  out[i]= '\0';
  return out;
}


/**
  Duplicate a SQLWCHAR as a SQLCHAR encoded as UTF-8.

  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in chars) or
                                destination string (in bytes)

  @return  Pointer to a newly allocated SQLCHAR, or @c NULL
*/
SQLCHAR *sqlwchar_as_utf8(SQLWCHAR *str, SQLINTEGER *len)
{
  SQLWCHAR *str_end;
  UTF8 *u8;
  SQLINTEGER i;

  if (*len == SQL_NTS)
    *len= sqlwcharlen(str);
  if (!str || *len == 0)
  {
    *len= 0;
    return NULL;
  }

  u8= (UTF8 *)my_malloc(sizeof(UTF8) * MAX_BYTES_PER_UTF8_CP * *len + 1,
                        MYF(0));
  if (!u8)
  {
    *len= -1;
    return NULL;
  }

  str_end= str + *len;

  if (sizeof(SQLWCHAR) == 4)
  {
    for (i= 0; str < str_end; )
      i+= utf32toutf8((UTF32)*str++, u8 + i);
  }
  else
  {
    for (i= 0; str < str_end; )
    {
      UTF32 u32;
      str+= utf16toutf32((UTF16 *)str, &u32);
      i+= utf32toutf8(u32, u8 + i);
    }
  }

  *len= i;
  u8[i]= '\0';
  return u8;
}


/**
  Convert a SQLCHAR encoded as UTF-8 into a SQLWCHAR.

  @param[out]     out           Pointer to SQLWCHAR buffer
  @param[in]      out_max       Length of @c out buffer
  @param[in]      in            Pointer to SQLCHAR string (utf-8 encoded)
  @param[in]      in_len        Length of @c in (in bytes)

  @return  Number of characters stored in the @c out buffer
*/
SQLSMALLINT utf8_as_sqlwchar(SQLWCHAR *out, SQLINTEGER out_max, SQLCHAR *in,
                             SQLINTEGER in_len)
{
  SQLINTEGER i;
  SQLWCHAR *pos, *out_end;

  for (i= 0, pos= out, out_end= out + out_max; i < in_len && pos < out_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
      i+= utf8toutf32(in + i, (UTF32 *)pos++);
    else
    {
      UTF32 u32;
      i+= utf8toutf32(in + i, &u32);
      pos+= utf32toutf16(u32, (UTF16 *)pos);
    }
  }

  if (pos)
    *pos= 0;
  return pos - out;
}


/**
  Duplicate a SQLCHAR as a SQLCHAR in the specified character set.

  @param[in]      from_charset  Character set to convert from
  @param[in]      to_charset    Character set to convert into
  @param[in]      str           String to convert
  @param[in,out]  len           Pointer to length of source (in chars) or
                                destination string (in bytes)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Pointer to a newly allocated SQLCHAR, or @c NULL
*/
SQLCHAR *sqlchar_as_sqlchar(CHARSET_INFO *from_charset,
                            CHARSET_INFO *to_charset,
                            SQLCHAR *str, SQLINTEGER *len, uint *errors)
{
  uint32 used_bytes, used_chars, bytes;
  SQLCHAR *conv;

  if (*len == SQL_NTS)
    *len= strlen((char *)str);

  bytes= (*len / from_charset->mbminlen * to_charset->mbmaxlen);
  conv= (SQLCHAR *)my_malloc(bytes + 1, MYF(0));
  if (!conv)
  {
    *len= -1;
    return NULL;
  }

  *len= copy_and_convert((char *)conv, bytes, to_charset,
                         (char *)str, *len,
                         from_charset, &used_bytes,
                         &used_chars, errors);

  conv[*len]= '\0';

  return conv;
}

 
/**
  Convert a SQLWCHAR to a SQLCHAR in the specified character set. This
  variation uses a pre-allocated buffer.

  @param[in]      charset_info  Character set to convert into
  @param[out]     out           Pointer to SQLWCHAR buffer
  @param[in]      out_bytes     Length of @c out buffer
  @param[in]      str           String to convert
  @param[in]      len           Length of @c in (in SQLWCHAR's)
  @param[out]     errors        Pointer to count of errors in conversion

  @return  Number of characters stored in the @c out buffer
*/
SQLINTEGER sqlwchar_as_sqlchar_buf(CHARSET_INFO *charset_info,
                                   SQLCHAR *out, SQLINTEGER out_bytes,
                                   SQLWCHAR *str, SQLINTEGER len, uint *errors)
{
  SQLWCHAR *str_end;
  SQLINTEGER i, u8_len;
  UTF8 u8[MAX_BYTES_PER_UTF8_CP + 1];
  uint32 used_bytes, used_chars;

  *errors= 0;

  if (len == SQL_NTS)
    len= sqlwcharlen(str);
  if (!str || len == 0)
    return 0;

  str_end= str + len;

  for (i= 0; str < str_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      u8_len= utf32toutf8((UTF32)*str++, u8);
    }
    else
    {
      UTF32 u32;
      str+= utf16toutf32((UTF16 *)str, &u32);
      u8_len= utf32toutf8(u32, u8);
    }

    i+= copy_and_convert((char *)out + i, out_bytes - i, charset_info,
                         (char *)u8, u8_len, utf8_charset_info, &used_bytes,
                         &used_chars, errors);
  }

  out[i]= '\0';

  for (i= 0; str < str_end; )
  {
    if (sizeof(SQLWCHAR) == 4)
    {
      u8_len= utf32toutf8((UTF32)*str++, u8);
    }
    else
    {
      UTF32 u32;
      str+= utf16toutf32((UTF16 *)str, &u32);
      u8_len= utf32toutf8(u32, u8);
    }

    i+= copy_and_convert((char *)out + i, out_bytes - i, charset_info,
                         (char *)u8, u8_len, utf8_charset_info, &used_bytes,
                         &used_chars, errors);
  }

  return i;
}


/**
  Copy a string from one character set to another. Taken from sql_string.cc
  in the MySQL Server source code, since we don't export this functionality
  in libmysql yet.

  @c to must be at least as big as @c from_length * @c to_cs->mbmaxlen

  @param[in,out] to           Store result here
  @param[in]     to_cs        Character set of result string
  @param[in]     from         Copy from here
  @param[in]     from_length  Length of string in @c from (in bytes)
  @param[in]     from_cs      Character set of string in @c from
  @param[out]    used_bytes   Buffer for returning number of bytes consumed
                              from @c from
  @param[out]    used_chars   Buffer for returning number of chars consumed
                              from @c from
  @param[in,out] errors       Pointer to value where number of errors
                              encountered is added.

  @retval Length of bytes copied to @c to
*/
uint32
copy_and_convert(char *to, uint32 to_length, CHARSET_INFO *to_cs,
                 const char *from, uint32 from_length, CHARSET_INFO *from_cs,
                 uint32 *used_bytes, uint32 *used_chars, uint *errors)
{
  int         from_cnvres, to_cnvres;
  my_wc_t     wc;
  const uchar *from_end= (const uchar*) from+from_length;
  char *to_start= to;
  uchar *to_end= (uchar*) to+to_length;
  int (*mb_wc)(struct charset_info_st *, my_wc_t *, const uchar *,
               const uchar *) = from_cs->cset->mb_wc;
  int (*wc_mb)(struct charset_info_st *, my_wc_t, uchar *s, uchar *e)=
    to_cs->cset->wc_mb;
  uint error_count= 0;

  *used_bytes= *used_chars= 0;

  while (1)
  {
    if ((from_cnvres= (*mb_wc)(from_cs, &wc, (uchar*) from, from_end)) > 0)
      from+= from_cnvres;
    else if (from_cnvres == MY_CS_ILSEQ)
    {
      error_count++;
      from++;
      wc= '?';
    }
    else if (from_cnvres > MY_CS_TOOSMALL)
    {
      /*
        A correct multibyte sequence detected
        But it doesn't have Unicode mapping.
      */
      error_count++;
      from+= (-from_cnvres);
      wc= '?';
    }
    else
      break; /* Not enough characters */

outp:
    if ((to_cnvres= (*wc_mb)(to_cs, wc, (uchar*) to, to_end)) > 0)
      to+= to_cnvres;
    else if (to_cnvres == MY_CS_ILUNI && wc != '?')
    {
      error_count++;
      wc= '?';
      goto outp;
    }
    else
      break;

    *used_bytes+= from_cnvres;
    *used_chars+= 1;
  }
  *errors+= error_count;

  return (uint32) (to - to_start);
}




/*
 * Compare two SQLWCHAR strings ignoring case. This is only
 * case-insensitive over the ASCII range of characters.
 *
 * @return 0 if the strings are the same, 1 if they are not.
 */
int sqlwcharcasecmp(const SQLWCHAR *s1, const SQLWCHAR *s2)
{
  SQLWCHAR c1, c2;
  while (*s1 && *s2)
  {
    c1= *s1;
    c2= *s2;
    /* capitalize both strings */
    if (c1 >= 'a')
      c1 -= ('a' - 'A');
    if (c2 >= 'a')
      c2 -= ('a' - 'A');
    if (c1 != c2)
      return 1;
    s1++;
    s2++;
  }

  /* one will be null, so both must be */
  return *s1 != *s2;
}


/*
 * Locate a SQLWCHAR in a SQLWCHAR string.
 *
 * @return Position of char if found, otherwise NULL.
 */
const SQLWCHAR *sqlwcharchr(const SQLWCHAR *wstr, SQLWCHAR wchr)
{
  while (*wstr != wchr && *wstr++);
  if (*wstr == wchr)
    return wstr;
  else
    return NULL;
}


/*
 * Calculate the length of a SQLWCHAR string.
 *
 * @return The number of SQLWCHAR units in the string.
 */
size_t sqlwcharlen(const SQLWCHAR *wstr)
{
  size_t len= 0;
  while (wstr && *wstr++)
    len++;
  return len;
}


/*
 * Duplicate a SQLWCHAR string. Memory is allocated with my_malloc()
 * and should be freed with my_free() or the x_free() macro.
 *
 * @return A pointer to a new string.
 */
SQLWCHAR *sqlwchardup(const SQLWCHAR *wstr, size_t charlen)
{
  size_t chars= charlen == SQL_NTS ? sqlwcharlen(wstr) : charlen;
  SQLWCHAR *res= (SQLWCHAR *)my_malloc((chars + 1) * sizeof(SQLWCHAR), MYF(0));
  if (!res)
    return NULL;
  memcpy(res, wstr, chars * sizeof(SQLWCHAR));
  res[chars]= 0;
  return res;
}


/*
 * Convert a SQLWCHAR string to a long integer.
 *
 * @return The integer result of the conversion or 0 if the
 *         string could not be parsed.
 */
ulong sqlwchartoul(const SQLWCHAR *wstr)
{
  const SQLWCHAR *end= wstr + sqlwcharlen(wstr) - 1;
  SQLWCHAR c;
  int pten= 1;
  unsigned long res= 0;
  if (!wstr)
    return 0;
  for (; end >= wstr; pten *= 10, end--)
  {
    c= *end;
    if (c < '0' || c > '9')
      return 0;
    res += (c - '0') * pten;
  }
  return res;
}


/*
 * Convert a long integer to a SQLWCHAR string.
 */
void sqlwcharfromul(SQLWCHAR *wstr, unsigned long v)
{
  int chars;
  unsigned long v1;
  for(chars= 0, v1= v; v1 > 0; chars++, v1 /= 10);
  wstr[chars]= (SQLWCHAR)0;
  for(v1= v; v1 > 0; v1 /= 10)
    wstr[--chars]= (SQLWCHAR)('0' + (v1 % 10));
}


/*
 * Concatenate two strings. This differs from the convential
 * strncat() in that the parameter n is reduced by the number
 * of characters used (including NULL).
 *
 * Returns the number of characters copied.
 */
size_t sqlwcharncat2(SQLWCHAR *dest, const SQLWCHAR *src, size_t *n)
{
  SQLWCHAR *orig_dest;
  if (!n || !*n)
    return 0;
  orig_dest = (dest += sqlwcharlen(dest));
  while (*src && *n && (*n)--)
    *dest++ = *src++;
  if (*n)
  {
    (*n)--;
    *dest= 0;
  }
  else
    *(dest - 1)= 0;
  return dest - orig_dest;
}

