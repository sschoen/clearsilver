
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "neo_err.h"
#include "neo_hdf.h"
#include "neo_str.h"

static NEOERR *_alloc_hdf (HDF **hdf, char *name, size_t nlen, char *value, 
    int dup)
{
  *hdf = calloc (1, sizeof (HDF));
  if (*hdf == NULL)
  {
    return nerr_raise (NERR_NOMEM, "Unable to allocate memory for hdf element");
  }

  if (name != NULL)
  {
    (*hdf)->name_len = nlen;
    (*hdf)->name = (char *) malloc (nlen + 1);
    if ((*hdf)->name == NULL)
    {
      free((*hdf));
      (*hdf) = NULL;
      return nerr_raise (NERR_NOMEM, 
	  "Unable to allocate memory for hdf element: %s", name);
    }
    strncpy((*hdf)->name, name, nlen);
    (*hdf)->name[nlen] = '\0';
  }
  if (value != NULL)
  {
    if (dup)
    {
      (*hdf)->alloc_value = 1;
      (*hdf)->value = strdup(value);
      if ((*hdf)->value == NULL)
      {
	free((*hdf)->name);
	free((*hdf));
	(*hdf) = NULL;
	return nerr_raise (NERR_NOMEM, 
	    "Unable to allocate memory for hdf element %s", name);
      }
    }
    else
    {
      (*hdf)->value = value;
    }
  }
  return STATUS_OK;
}

static void _dealloc_hdf (HDF **hdf)
{
  if (*hdf == NULL) return;
  if ((*hdf)->child != NULL)
    _dealloc_hdf(&((*hdf)->child));
  if ((*hdf)->next != NULL)
    _dealloc_hdf(&((*hdf)->child));
  if ((*hdf)->name != NULL)
  {
    free ((*hdf)->name);
    (*hdf)->name = NULL;
  }
  if ((*hdf)->value != NULL)
  {
    if ((*hdf)->alloc_value)
      free ((*hdf)->value);
    (*hdf)->value = NULL;
  }
  free (*hdf);
  *hdf = NULL;
}

NEOERR* hdf_init (HDF **hdf)
{
  NEOERR *err;
  HDF *my_hdf;

  *hdf = NULL;

  err = nerr_init();
  if (err != STATUS_OK)
    return nerr_pass (err);

  err = _alloc_hdf (&my_hdf, NULL, 0, NULL, 0);
  if (err != STATUS_OK)
    return nerr_pass (err);

  my_hdf->top = 1;

  *hdf = my_hdf;

  return STATUS_OK;
}

void hdf_destroy (HDF **hdf)
{
  if ((*hdf)->top)
    _dealloc_hdf(hdf);
}

static int _walk_hdf (HDF *hdf, char *name, HDF **node)
{
  HDF *hp = hdf;
  int x = 0;
  char *s = name;
  char *n = name;

  *node = NULL;

  hp = hdf->child;
  if (hp == NULL)
  {
    return -1;
  }

  n = name;
  s = strchr (n, '.');
  x = (s == NULL) ? strlen(n) : s - n;

  while (1)
  {
    while (hp != NULL)
    {
      if (hp->name && (x == hp->name_len) && !strncmp(hp->name, n, x))
      {
	break;
      }
      else
      {
	hp = hp->next;
      }
    }
    if (hp == NULL)
    {
      return -1;
    }
    if (s == NULL) break;
   
    hp = hp->child;
    n = s + 1;
    s = strchr (n, '.');
    x = (s == NULL) ? strlen(n) : s - n;
  } 
  *node = hp;
  return 0;
}

int hdf_get_int_value (HDF *hdf, char *name, int defval)
{
  HDF *node;
  int v;

  if ((_walk_hdf(hdf, name, &node) == 0) && (node->value != NULL))
  {
    v = atoi(node->value);
    return v;
  }
  return defval;
}

char* hdf_get_value (HDF *hdf, char *name, char *defval)
{
  HDF *node;

  if ((_walk_hdf(hdf, name, &node) == 0) && (node->value != NULL))
  {
    return node->value;
  }
  return defval;
}

NEOERR* hdf_get_copy (HDF *hdf, char *name, char **value, char *defval)
{
  HDF *node;

  if ((_walk_hdf(hdf, name, &node) == 0) && (node->value != NULL))
  {
    *value = strdup(node->value);
    if (*value == NULL)
    {
      return nerr_raise (NERR_NOMEM, "Unable to allocate copy of %s", name);
    }
  }
  else
  {
    if (defval == NULL)
      *value = NULL;
    else
    {
      *value = strdup(defval);
      if (*value == NULL)
      {
	return nerr_raise (NERR_NOMEM, "Unable to allocate copy of %s", name);
      }
    }
  }
  return STATUS_OK;
}

HDF* hdf_get_obj (HDF *hdf, char *name)
{
  HDF *obj;

  _walk_hdf(hdf, name, &obj);
  return obj;
}

HDF* hdf_obj_child (HDF *hdf)
{
  return hdf->child;
}

HDF* hdf_obj_next (HDF *hdf)
{
  return hdf->next;
}

char* hdf_obj_name (HDF *hdf)
{
  return hdf->name;
}

char* hdf_obj_value (HDF *hdf)
{
  return hdf->value;
}

NEOERR* _set_value (HDF *hdf, char *name, char *value, int dup)
{
  NEOERR *err;
  HDF *hn, *hp, *hs;
  int x = 0;
  char *s = name;
  char *n = name;

  if (hdf == NULL || name == NULL)
  {
    return nerr_raise(NERR_ASSERT, "Unable to set %s on NULL hdf", name);
  }

  n = name;
  s = strchr (n, '.');
  x = (s != NULL) ? s - n : strlen(n);
  if (x == 0)
  {
    return nerr_raise(NERR_ASSERT, "Unable to set Empty component %s", name);
  }

  hn = hdf;

  while (1)
  {
    hp = hn->child;
    hs = NULL;

    while (hp != NULL)
    {
      if (hp->name && (x == hp->name_len) && !strncmp(hp->name, n, x))
      {
	break;
      }
      hs = hp;
      hp = hp->next;
    }
    if (hp == NULL)
    {
      if (s != NULL)
      {
	err = _alloc_hdf (&hp, n, x, NULL, 0);
      }
      else
      {
	err = _alloc_hdf (&hp, n, x, value, dup);
      }
      if (err != STATUS_OK)
	return nerr_pass (err);
      if (hn->child == NULL)
	hn->child = hp;
      else
	hs->next = hp;
    }
    else if (s == NULL)
    {
      if (hp->alloc_value)
      {
	free(hp->value);
      }
      if (dup)
      {
	hp->alloc_value = 1;
	hp->value = strdup(value);
	if (hp->value == NULL)
	  return nerr_raise (NERR_NOMEM, "Unable to duplicate value %s for %s", 
	      value, name);
      }
      else
      {
	hp->alloc_value = 0;
	hp->value = value;
      }
    }
    if (s == NULL)
      break;
    n = s + 1;
    s = strchr (n, '.');
    x = (s != NULL) ? s - n : strlen(n);
    if (x == 0)
    {
      return nerr_raise(NERR_ASSERT, "Unable to set Empty component %s", name);
    }
    hn = hp;
  }
  return STATUS_OK;
}

NEOERR* hdf_set_value (HDF *hdf, char *name, char *value)
{
  return nerr_pass(_set_value (hdf, name, value, 1));
}

NEOERR* hdf_set_buf (HDF *hdf, char *name, char *value)
{
  return nerr_pass(_set_value (hdf, name, value, 0));
}

NEOERR* hdf_set_copy (HDF *hdf, char *dest, char *src)
{
  HDF *node;
  if ((_walk_hdf(hdf, src, &node) == 0) && (node->value != NULL))
  {
    return nerr_pass(_set_value (hdf, dest, node->value, 0));
  }
  return nerr_raise (NERR_NOT_FOUND, "Unable to find %s", src);
}

NEOERR* hdf_dump(HDF *hdf, char *prefix)
{
  char *p;

  if (hdf->value)
  {
    if (prefix)
    {
      printf("%s.%s = %s\n", prefix, hdf->name, hdf->value);
    }
    else
    {
      printf("%s = %s\n", hdf->name, hdf->value);
    }
  }
  if (hdf->child)
  {
    if (prefix)
    {
      p = (char *) malloc (strlen(hdf->name) + strlen(prefix) + 2);
      sprintf (p, "%s.%s", prefix, hdf->name);
      hdf_dump(hdf->child, p);
      free(p);
    }
    else
    {
      hdf_dump(hdf->child, hdf->name);
    }
  }
  if (hdf->next)
  {
    hdf_dump(hdf->next, prefix);
  }
  return STATUS_OK;
}

/* HDF file looks like the following: */
#define SKIPWS(s) while (*s && isspace(*s)) s++;

static NEOERR* hdf_read_file_fp (HDF *hdf, FILE *fp, char *path, int *line)
{
  NEOERR *err;
  HDF *lower;
  char buf[4096];
  char *s;
  char *name, *value;
  int l;

  while (fgets(buf, sizeof(buf), fp) != NULL)
  {
    (*line)++;
    s = buf;
    SKIPWS(s);
    if (!strncmp(s, "#include ", 9))
    {
      s += 9;
      name = neos_strip(s);
      l = strlen(name);
      if (name[0] == '"' && name[l-1] == '"')
      {
	name[l-1] = '\0';
	name++;
      }
      err = hdf_read_file(hdf, name);
      if (err != STATUS_OK) 
	return nerr_pass_ctx(err, "In file %s:%d", path, *line);
    }
    else if (s[0] == '#')
    {
      /* comment: pass */
    }
    else if (s[0] == '}') /* up */
    {
      s = neos_strip(s);
      if (strcmp(s, "}"))
      {
	return nerr_raise(NERR_PARSE, 
	    "[%s:%d] Trailing garbage on line following }: %s", path, *line,
	    buf);
      }
      return STATUS_OK;
    }
    else if (s[0])
    {
      /* Valid hdf name is [0-9a-zA-Z_.]+ */
      name = s;
      while (*s && (isalnum(*s) || *s == '_' || *s == '.')) s++;
      if (*s != '\0')
      {
	*s++ = '\0';
      }
      SKIPWS(s);

      if (s[0] == '=') /* assignment */
      {
	s++;
	value = neos_strip(s);
	err = hdf_set_value (hdf, name, value);
	if (err != STATUS_OK)
	  return nerr_pass_ctx(err, "In file %s:%d", path, *line);
      }
      else if (s[0] == ':') /* copy */
      {
	s++;
	value = neos_strip(s);
	err = hdf_set_copy (hdf, name, value);
	if (err != STATUS_OK)
	  return nerr_pass_ctx(err, "In file %s:%d", path, *line);
      }
      else if (s[0] == '{') /* deeper */
      {
	lower = hdf_get_obj (hdf, name);
	if (lower == NULL)
	{
	  err = hdf_set_value (hdf, name, NULL);
	  if (err != STATUS_OK) 
	    return nerr_pass_ctx(err, "In file %s:%d", path, *line);
	  lower = hdf_get_obj (hdf, name);
	}
	err = hdf_read_file_fp(lower, fp, path, line);
	if (err != STATUS_OK) 
	  return nerr_pass_ctx(err, "In file %s:%d", path, *line);
	if (feof(fp)) break;
      }
      else if (s[0] == '<' && s[1] == '<') /* multi-line assignment */
      {
	char *m;
	int msize = 0;
	int mmax = 128;
	int l;
	s+=2;
	value = neos_strip(s);
	l = strlen(value);
	if (l == 0)
	  return nerr_raise(NERR_PARSE, 
	      "[%s:%d] No multi-assignment terminator given: %s", path, *line, 
	      buf);
	m = (char *) malloc (mmax * sizeof(char));
	if (m == NULL)
	  return nerr_raise(NERR_NOMEM, 
	    "[%s:%d] Unable to allocate memory for multi-line assignment to %s",
	    path, *line, name);
	while (fgets(m+msize, mmax-msize, fp) != NULL)
	{
	  if (!strncmp(value, m+msize, l) && isspace(m[msize+l]))
	  {
	    m[msize] = '\0';
	    break;
	  }
	  msize += strlen(m+msize);
	  if (msize + l + 10 > mmax)
	  {
	    mmax += 128;
	    m = (char *) realloc (m, mmax * sizeof(char));
	    if (m == NULL)
	      return nerr_raise(NERR_NOMEM, 
		  "[%s:%d] Unable to allocate memory for multi-line assignment to %s: size=%d",
		  path, *line, name, mmax);
	  }
	}
	err = hdf_set_buf(hdf, name, m);
	if (err != STATUS_OK)
	{
	  free (m);
	  return nerr_pass_ctx(err, "In file %s:%d", path, *line);
	}

      }
      else
      {
	return nerr_raise(NERR_PARSE, "[%s:%d] Unable to parse line %s",
	    path, *line, buf);
      }
    }
  }
  return STATUS_OK;
}

NEOERR* hdf_read_file (HDF *hdf, char *path)
{
  FILE *fp;
  int line = 0;

  fp = fopen(path, "r");
  if (fp == NULL)
    return nerr_raise(NERR_IO, "Unable to open file %s: [%d] %s", path,
	errno, strerror(errno));

  return nerr_pass(hdf_read_file_fp(hdf, fp, path, &line));
}

