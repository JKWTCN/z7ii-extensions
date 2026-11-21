/* SPDX-License-Identifier: MIT */

/* The mux task is registered in RAM. Its close path calls the header builder
 * directly, so the three copies retain native event handling and file writes.
 * Offsets are relative to each copy; untouched PC-relative loads use their
 * original literal pool offsets. The task and close copies include that span. */
enum { MROT_TASK_SIZE=0x418, MROT_CLOSE_SIZE=0x264, MROT_HEADER_SIZE=0x7c };

static const z72_u32 movie_rot_task_calls[][2]={
    {0x022u,0x104de969u}, {0x02au,0x104dfe1fu}, {0x036u,0x104de969u}, {0x03eu,0x104dea9fu},
    {0x086u,0x104de969u}, {0x08eu,0x104df181u}, {0x096u,0x104dfe0du}, {0x0a2u,0x104de969u},
    {0x0aau,0x104dea9fu}, {0x0eau,0x104de969u}, {0x0f2u,0x104df061u}, {0x0fau,0x104dfd7du},
    {0x104u,0x104dfdfbu}, {0x110u,0x104de969u}, {0x118u,0x104dea9fu}, {0x158u,0x104de969u},
    {0x160u,0x104defddu}, {0x168u,0x104dfdedu}, {0x174u,0x104de969u}, {0x17cu,0x104dea9fu},
    {0x1bau,0x104de969u}, {0x1cau,0x104dfdabu}, {0x1d6u,0x104de969u}, {0x1deu,0x104dea9fu},
    {0x220u,0x104de969u}, {0x23cu,0x104dfd9bu}, {0x248u,0x104de969u}, {0x256u,0x104dea9fu},
    {0x25eu,0x104de9efu}, {0x26eu,0x104dea31u}, {0x286u,0x104de9efu}, {0x2a2u,0x104de9a3u},
    {0x2aeu,0x104de969u},
};
static const z72_u32 movie_rot_close_calls[][2]={
    {0x00cu,0x10360f19u}, {0x030u,0x10360f44u}, {0x038u,0x10360f13u}, {0x042u,0x104dfd9bu},
    {0x058u,0x104dfd7du}, {0x070u,0x10520975u}, {0x076u,0x104deac7u}, {0x086u,0x10536d83u},
    {0x08cu,0x10520f63u}, {0x090u,0x104deac7u}, {0x09au,0x10537045u}, {0x0a2u,0x1053706fu},
    {0x0aau,0x10520f57u}, {0x0aeu,0x104deac7u}, {0x0b6u,0x10536d83u}, {0x0c2u,0x104deac7u},
    {0x0cau,0x10536d9bu}, {0x0d4u,0x10360f13u}, {0x0dau,0x104dfd9bu}, {0x0f6u,0x10536c49u},
    {0x114u,0x104de9a3u},
};
static const z72_u32 movie_rot_header_calls[][2]={
    {0x008u,0x10539665u}, {0x010u,0x10539485u}, {0x028u,0x105385fbu}, {0x034u,0x10537f63u},
    {0x046u,0x1053a855u}, {0x04eu,0x1053a8c5u}, {0x058u,0x10537bc7u}, {0x060u,0x105372bfu},
    {0x072u,0x10537219u},
};

static _Alignas(64) unsigned char movie_rot_task[MROT_TASK_SIZE+sizeof(movie_rot_task_calls)+24];
static _Alignas(64) unsigned char movie_rot_close[MROT_CLOSE_SIZE+sizeof(movie_rot_close_calls)+8];
static _Alignas(64) unsigned char movie_rot_header[MROT_HEADER_SIZE+sizeof(movie_rot_header_calls)+8];

static z72_u32 movie_rot_relocate(unsigned char *code, z72_u32 size,
                                  const z72_u32 calls[][2], z72_u32 count)
{
    z72_u32 stub=(z72_u32)code+size;
    for (z72_u32 i=0;i<count;++i,stub+=8u)
        write_call((z72_u32)code+calls[i][0],stub,calls[i][1]);
    return stub;
}

static z72_u32 movie_rot_prepare(void)
{
    if (MROT_WORD(MROT_CONTROL_SLOT)!=MROT_CONTROL || MROT_WORD(MROT_TASK_SLOT)!=MROT_TASK_NATIVE ||
        copy_code(movie_rot_task,MROT_TASK_NATIVE&~1u,MROT_TASK_SIZE,0xae9e7b13u) ||
        copy_code(movie_rot_close,0x104deaf0u,MROT_CLOSE_SIZE,0x9832d685u) ||
        copy_code(movie_rot_header,0x10539794u,MROT_HEADER_SIZE,0x32842c20u)) return 0;

    z72_u32 stub=movie_rot_relocate(movie_rot_task,MROT_TASK_SIZE,movie_rot_task_calls,
                                    MROT_COUNT(movie_rot_task_calls));
    write_call((z72_u32)movie_rot_task+0x04au,stub,(z72_u32)movie_rot_open);
    write_call((z72_u32)movie_rot_task+0x1c2u,stub+8u,(z72_u32)movie_rot_split);
    write_call((z72_u32)movie_rot_task+0x22cu,stub+16u,(z72_u32)movie_rot_close|1u);
    stub=movie_rot_relocate(movie_rot_close,MROT_CLOSE_SIZE,movie_rot_close_calls,
                           MROT_COUNT(movie_rot_close_calls));
    write_call((z72_u32)movie_rot_close+0x0beu,stub,(z72_u32)movie_rot_header|1u);
    stub=movie_rot_relocate(movie_rot_header,MROT_HEADER_SIZE,movie_rot_header_calls,
                           MROT_COUNT(movie_rot_header_calls));
    write_call((z72_u32)movie_rot_header+0x01cu,stub,(z72_u32)movie_rot_track);
    return 1;
}

static void movie_rot_install(void)
{
    if (!movie_rot_prepare()) return;
    clean_code(movie_rot_task,sizeof(movie_rot_task));
    clean_code(movie_rot_close,sizeof(movie_rot_close));
    clean_code(movie_rot_header,sizeof(movie_rot_header));
    sync_code();
    MROT_WORD(MROT_TASK_SLOT)=(z72_u32)movie_rot_task|1u;
}
