#pragma once

enum metadata_key {
    METADATA_ARTIST,
    METADATA_ALBUM,
    METADATA_TITLE,
    METADATA_DATE,
    METADATA_ORIGINALDATE,
    METADATA_GENRE,
    METADATA_DISC,
    METADATA_TRACK,
    METADATA_ALBUMARTIST,
    METADATA_COMPILATION,
    METADATA_REMIXER,
    METADATA_BPM,

    METADATA_INVALID,
};

#define METADATA_NUM_TAGS METADATA_INVALID

struct metadata_pair {
    enum metadata_key key;
    char *val;
};

typedef char metadata_date_format[sizeof("20120425")];

struct metadata {
    _Atomic size_t refcount;
    
    char *artist;
    char *albumartist;
    char *title;
    char *album;
    char *genre;
    i32 year;

    struct metadata_pair pairs[];
};

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
