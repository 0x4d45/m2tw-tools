import std.mem; 
import type.magic;

struct Record
{
    u32 data_offset;
    u32 first_chunk;
    u32 size_on_disk;
    u32 size_in_pack;
    char path[];
    std::mem::AlignTo<4>;
    u8 data[size_in_pack] @ data_offset;
};

struct Pack
{
    type::Magic<"PACK"> magic;
    u8 version[4];
    u32 num_records;
    u32 size_of_records;
    u32 num_chunks;
    u32 record_offsets[num_records];
    u32 chunk_sizes[num_chunks];
    Record records[num_records];
} [[inline]];

Pack pack @ 0x00;
