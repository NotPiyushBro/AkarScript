// Akar .ako Disassembler — standalone tool
// Usage: akar_disasm <file.ako>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>

static const char* op_names[] = {
    "LOAD_CONST","LOAD_NIL","LOAD_TRUE","LOAD_FALSE","MOVE",
    "GET_LOCAL","SET_LOCAL","GET_UPVALUE","SET_UPVALUE","GET_GLOBAL","SET_GLOBAL",
    "ADD","SUB","MUL","DIV","MOD","NEG","EQ","NEQ","LT","LTE","GT","GTE","NOT",
    "JMP","JMP_IF_FALSE","JMP_IF_TRUE","CALL","CLOSURE","CLOSE_UPVALUE","RETURN",
    "NEW_ARRAY","NEW_MAP","GET_INDEX","SET_INDEX","GET_FIELD","SET_FIELD",
    "NEW_CLASS","NEW_INSTANCE","GET_METHOD","INVOKE","NEW_RANGE",
    "ITER_INIT","ITER_NEXT","ITER_DONE","PRINT","HALT","NOP",
    "ADD_NUM","SUB_NUM","MUL_NUM","DIV_NUM","MOD_NUM","ADD_STR",
    "EQ_NUM","NEQ_NUM","LT_NUM","LTE_NUM","GT_NUM","GTE_NUM",
    "MOD_EQ_ZERO","FIBER_YIELD","FIBER_RESUME","TAIL_CALL","AWAIT",
    "THROW","TRY_BEGIN","TRY_END","WIDE",
    "SIGNAL_CREATE","SIGNAL_GET","SIGNAL_SET",
    "EFFECT_CREATE","EFFECT_RUN",
    "ENUM_CREATE","ENUM_VARIANT","ENUM_DATA_VARIANT",
    "ENUM_GET","ENUM_IS",
};

static const char* opn(uint8_t op) {
    return (op < sizeof(op_names)/sizeof(op_names[0])) ? op_names[op] : "???";
}

// Big-endian readers for section data
static uint8_t ru8(const uint8_t* d, size_t& p) { return d[p++]; }
static uint16_t ru16(const uint8_t* d, size_t& p) { uint16_t v=(d[p]<<8)|d[p+1]; p+=2; return v; }
static uint32_t ru32(const uint8_t* d, size_t& p) { uint32_t v=(d[p]<<24)|(d[p+1]<<16)|(d[p+2]<<8)|d[p+3]; p+=4; return v; }
static double rf64(const uint8_t* d, size_t& p) {
    uint64_t bits=0; for(int i=0;i<8;i++) bits=(bits<<8)|d[p+i]; p+=8;
    double v; std::memcpy(&v,&bits,8); return v;
}
static std::string rstr(const uint8_t* d, size_t& p) {
    uint32_t len=ru32(d,p); std::string s((const char*)(d+p),len); p+=len; return s;
}

// Constants
enum CType { CNil, CBool, CNum, CStrRef, CFunc };
struct Const {
    CType type; bool bval; double nval; uint32_t sidx;
    std::string fname; uint16_t farity, fregs; bool fvarargs;
    std::vector<uint8_t> fbc; std::vector<Const> fconsts;
};

static Const rc(const uint8_t* d, size_t& p) {
    Const c{}; uint8_t tag=ru8(d,p);
    if(tag==0) { c.type=CNil; }
    else if(tag==1) { c.type=CBool; c.bval=ru8(d,p)!=0; }
    else if(tag==2) { c.type=CNum; c.nval=rf64(d,p); }
    else if(tag==3) {
        uint8_t sub=ru8(d,p);
        if(sub==0) { c.type=CStrRef; c.sidx=ru32(d,p); }
        else if(sub==1) {
            c.type=CFunc; c.fname=rstr(d,p); c.farity=ru16(d,p); c.fregs=ru16(d,p);
            c.fvarargs=ru8(d,p)!=0;
            uint32_t uv=ru32(d,p); for(uint32_t i=0;i<uv;i++){ru8(d,p);ru8(d,p);}
            uint32_t cc=ru32(d,p); for(uint32_t i=0;i<cc;i++) c.fconsts.push_back(rc(d,p));
            uint32_t bsz=ru32(d,p); c.fbc.assign(d+p,d+p+bsz); p+=bsz;
        } else c.type=CNil;
    } else c.type=CNil;
    return c;
}

static std::string fmt(const Const& c, const std::vector<std::string>& strs) {
    char buf[128];
    switch(c.type) {
        case CNil: return "nil";
        case CBool: return c.bval?"true":"false";
        case CNum: snprintf(buf,128,"%g",c.nval); return buf;
        case CStrRef: return c.sidx<strs.size() ? "\""+strs[c.sidx]+"\"" : "<str>";
        case CFunc: return "<fn "+c.fname+">";
    } return "?";
}

static void disasm(const std::string& label, const std::vector<uint8_t>& bc,
                   const std::vector<Const>& consts, const std::vector<std::string>& strs, int depth) {
    std::string pad(depth*2,' ');
    printf("%s=== %s === (%zu constants, %zu bytes)\n", pad.c_str(), label.c_str(), consts.size(), bc.size());
    for(size_t i=0;i<consts.size();i++)
        printf("%s  [%zu] %s\n", pad.c_str(), i, fmt(consts[i],strs).c_str());
    printf("%sBytecode:\n", pad.c_str());

    size_t pos=0;
    while(pos<bc.size()) {
        size_t addr=pos;
        uint8_t op=bc[pos++];

        if(op==68) { // WIDE
            if(pos+7>bc.size()){printf("%s  %4zu: WIDE <truncated>\n",pad.c_str(),addr);break;}
            uint8_t wop=bc[pos++];
            uint16_t wa=(bc[pos]<<8)|bc[pos+1]; pos+=2;
            uint16_t wb=(bc[pos]<<8)|bc[pos+1]; pos+=2;
            uint16_t wc=(bc[pos]<<8)|bc[pos+1]; pos+=2;
            printf("%s  %4zu: WIDE %s R%u R%u R%u\n", pad.c_str(), addr, opn(wop), wa, wb, wc);
            continue;
        }

        uint8_t a=pos<bc.size()?bc[pos++]:0;
        uint8_t b=pos<bc.size()?bc[pos++]:0;
        uint8_t c=pos<bc.size()?bc[pos++]:0;
        uint16_t bx=(b<<8)|c;
        int16_t sbx=(int16_t)bx;

        switch(op) {
        case 0: { // LOAD_CONST
            std::string v=(bx<consts.size())?fmt(consts[bx],strs):"?";
            printf("%s  %4zu: %-14s R%-3u #%-3u %s\n",pad.c_str(),addr,"LOAD_CONST",a,bx,v.c_str());
            break; }
        case 1: printf("%s  %4zu: LOAD_NIL      R%u\n",pad.c_str(),addr,a); break;
        case 2: printf("%s  %4zu: LOAD_TRUE     R%u\n",pad.c_str(),addr,a); break;
        case 3: printf("%s  %4zu: LOAD_FALSE    R%u\n",pad.c_str(),addr,a); break;
        case 4: printf("%s  %4zu: MOVE          R%u <- R%u\n",pad.c_str(),addr,a,b); break;
        case 5: printf("%s  %4zu: GET_LOCAL     R%u <- [BP+%u]\n",pad.c_str(),addr,a,b); break;
        case 6: printf("%s  %4zu: SET_LOCAL     [BP+%u] <- R%u\n",pad.c_str(),addr,b,a); break;
        case 7: printf("%s  %4zu: GET_UPVALUE   R%u <- upval[%u]\n",pad.c_str(),addr,a,b); break;
        case 8: printf("%s  %4zu: SET_UPVALUE   upval[%u] <- R%u\n",pad.c_str(),addr,b,a); break;
        case 9: { std::string v=(bx<consts.size())?fmt(consts[bx],strs):"?";
            printf("%s  %4zu: %-14s R%-3u #%-3u %s\n",pad.c_str(),addr,"GET_GLOBAL",a,bx,v.c_str()); break; }
        case 10: { std::string v=(bx<consts.size())?fmt(consts[bx],strs):"?";
            printf("%s  %4zu: %-14s #%-3u %s <- R%u\n",pad.c_str(),addr,"SET_GLOBAL",bx,v.c_str(),a); break; }
        case 11: printf("%s  %4zu: ADD           R%u = R%u + R%u\n",pad.c_str(),addr,a,b,c); break;
        case 12: printf("%s  %4zu: SUB           R%u = R%u - R%u\n",pad.c_str(),addr,a,b,c); break;
        case 13: printf("%s  %4zu: MUL           R%u = R%u * R%u\n",pad.c_str(),addr,a,b,c); break;
        case 14: printf("%s  %4zu: DIV           R%u = R%u / R%u\n",pad.c_str(),addr,a,b,c); break;
        case 15: printf("%s  %4zu: MOD           R%u = R%u %% R%u\n",pad.c_str(),addr,a,b,c); break;
        case 16: printf("%s  %4zu: NEG           R%u = -R%u\n",pad.c_str(),addr,a,b); break;
        case 17: printf("%s  %4zu: EQ            R%u = R%u == R%u\n",pad.c_str(),addr,a,b,c); break;
        case 18: printf("%s  %4zu: NEQ           R%u = R%u != R%u\n",pad.c_str(),addr,a,b,c); break;
        case 19: printf("%s  %4zu: LT            R%u = R%u < R%u\n",pad.c_str(),addr,a,b,c); break;
        case 20: printf("%s  %4zu: LTE           R%u = R%u <= R%u\n",pad.c_str(),addr,a,b,c); break;
        case 21: printf("%s  %4zu: GT            R%u = R%u > R%u\n",pad.c_str(),addr,a,b,c); break;
        case 22: printf("%s  %4zu: GTE           R%u = R%u >= R%u\n",pad.c_str(),addr,a,b,c); break;
        case 23: printf("%s  %4zu: NOT           R%u = !R%u\n",pad.c_str(),addr,a,b); break;
        case 24: printf("%s  %4zu: JMP           %+d -> %zu\n",pad.c_str(),addr,sbx,(size_t)(addr+4+sbx)); break;
        case 25: printf("%s  %4zu: JMP_IF_FALSE  R%u  %+d -> %zu\n",pad.c_str(),addr,a,sbx,(size_t)(addr+4+sbx)); break;
        case 26: printf("%s  %4zu: JMP_IF_TRUE   R%u  %+d -> %zu\n",pad.c_str(),addr,a,sbx,(size_t)(addr+4+sbx)); break;
        case 27: printf("%s  %4zu: CALL          R%u  %u args\n",pad.c_str(),addr,a,b); break;
        case 28: { std::string v=(bx<consts.size())?fmt(consts[bx],strs):"?";
            printf("%s  %4zu: %-14s R%-3u #%-3u %s\n",pad.c_str(),addr,"CLOSURE",a,bx,v.c_str()); break; }
        case 29: printf("%s  %4zu: CLOSE_UPVALUE R%u\n",pad.c_str(),addr,a); break;
        case 30: printf("%s  %4zu: RETURN        R%u\n",pad.c_str(),addr,a); break;
        case 31: printf("%s  %4zu: NEW_ARRAY     R%u  %u elems\n",pad.c_str(),addr,a,b); break;
        case 32: printf("%s  %4zu: NEW_MAP       R%u\n",pad.c_str(),addr,a); break;
        case 33: printf("%s  %4zu: GET_INDEX     R%u = R%u[R%u]\n",pad.c_str(),addr,a,b,c); break;
        case 34: printf("%s  %4zu: SET_INDEX     R%u[R%u] = R%u\n",pad.c_str(),addr,a,b,c); break;
        case 35: { std::string v=(c<consts.size())?fmt(consts[c],strs):"?";
            printf("%s  %4zu: %-14s R%u = R%u.%s\n",pad.c_str(),addr,"GET_FIELD",a,b,v.c_str()); break; }
        case 36: { std::string v=(b<consts.size())?fmt(consts[b],strs):"?";
            printf("%s  %4zu: %-14s R%u.%s = R%u\n",pad.c_str(),addr,"SET_FIELD",a,v.c_str(),c); break; }
        case 37: { std::string v=(bx<consts.size())?fmt(consts[bx],strs):"?";
            printf("%s  %4zu: %-14s R%-3u #%-3u %s\n",pad.c_str(),addr,"NEW_CLASS",a,bx,v.c_str()); break; }
        case 38: printf("%s  %4zu: NEW_INSTANCE  R%u = new R%u\n",pad.c_str(),addr,a,b); break;
        case 39: { std::string v=(c<consts.size())?fmt(consts[c],strs):"?";
            printf("%s  %4zu: %-14s R%u = R%u.%s\n",pad.c_str(),addr,"GET_METHOD",a,b,v.c_str()); break; }
        case 40: printf("%s  %4zu: INVOKE        R%u R%u R%u\n",pad.c_str(),addr,a,b,c); break;
        case 41: printf("%s  %4zu: NEW_RANGE     R%u = R%u..R%u\n",pad.c_str(),addr,a,b,c); break;
        case 42: printf("%s  %4zu: ITER_INIT     R%u = iter(R%u)\n",pad.c_str(),addr,a,b); break;
        case 43: printf("%s  %4zu: ITER_NEXT     R%u = next(R%u)\n",pad.c_str(),addr,a,b); break;
        case 44: printf("%s  %4zu: ITER_DONE     R%u = done(R%u)\n",pad.c_str(),addr,a,b); break;
        case 45: printf("%s  %4zu: PRINT         R%u\n",pad.c_str(),addr,a); break;
        case 46: printf("%s  %4zu: HALT\n",pad.c_str(),addr); break;
        case 47: printf("%s  %4zu: NOP\n",pad.c_str(),addr); break;
        case 48: printf("%s  %4zu: ADD_NUM       R%u = R%u + R%u\n",pad.c_str(),addr,a,b,c); break;
        case 49: printf("%s  %4zu: SUB_NUM       R%u = R%u - R%u\n",pad.c_str(),addr,a,b,c); break;
        case 50: printf("%s  %4zu: MUL_NUM       R%u = R%u * R%u\n",pad.c_str(),addr,a,b,c); break;
        case 51: printf("%s  %4zu: DIV_NUM       R%u = R%u / R%u\n",pad.c_str(),addr,a,b,c); break;
        case 52: printf("%s  %4zu: MOD_NUM       R%u = R%u %% R%u\n",pad.c_str(),addr,a,b,c); break;
        case 53: printf("%s  %4zu: ADD_STR       R%u = R%u .. R%u\n",pad.c_str(),addr,a,b,c); break;
        case 54: printf("%s  %4zu: EQ_NUM        R%u = R%u == R%u\n",pad.c_str(),addr,a,b,c); break;
        case 55: printf("%s  %4zu: NEQ_NUM       R%u = R%u != R%u\n",pad.c_str(),addr,a,b,c); break;
        case 56: printf("%s  %4zu: LT_NUM        R%u = R%u < R%u\n",pad.c_str(),addr,a,b,c); break;
        case 57: printf("%s  %4zu: LTE_NUM       R%u = R%u <= R%u\n",pad.c_str(),addr,a,b,c); break;
        case 58: printf("%s  %4zu: GT_NUM        R%u = R%u > R%u\n",pad.c_str(),addr,a,b,c); break;
        case 59: printf("%s  %4zu: GTE_NUM       R%u = R%u >= R%u\n",pad.c_str(),addr,a,b,c); break;
        case 60: printf("%s  %4zu: MOD_EQ_ZERO   R%u = (R%u %% R%u == 0)\n",pad.c_str(),addr,a,b,c); break;
        case 61: printf("%s  %4zu: FIBER_YIELD   R%u\n",pad.c_str(),addr,a); break;
        case 62: printf("%s  %4zu: FIBER_RESUME  R%u R%u R%u\n",pad.c_str(),addr,a,b,c); break;
        case 63: printf("%s  %4zu: TAIL_CALL     R%u %u args\n",pad.c_str(),addr,a,b); break;
        case 64: printf("%s  %4zu: AWAIT         R%u\n",pad.c_str(),addr,a); break;
        case 65: printf("%s  %4zu: THROW         R%u\n",pad.c_str(),addr,a); break;
        case 66: printf("%s  %4zu: TRY_BEGIN     R%u %+d -> %zu\n",pad.c_str(),addr,a,sbx,(size_t)(addr+4+sbx)); break;
        case 67: printf("%s  %4zu: TRY_END       %u\n",pad.c_str(),addr,bx); break;
        case 68: printf("%s  %4zu: WIDE\n",pad.c_str(),addr); break;
        case 69: printf("%s  %4zu: SIGNAL_CREATE R%u <- R%u\n",pad.c_str(),addr,a,b); break;
        case 70: printf("%s  %4zu: SIGNAL_GET    R%u <- sig[R%u]\n",pad.c_str(),addr,a,b); break;
        case 71: printf("%s  %4zu: SIGNAL_SET    sig[R%u] <- R%u\n",pad.c_str(),addr,a,b); break;
        case 72: printf("%s  %4zu: EFFECT_CREATE R%u <- effect(R%u)\n",pad.c_str(),addr,a,b); break;
        case 73: printf("%s  %4zu: EFFECT_RUN    R%u\n",pad.c_str(),addr,a); break;
        case 74: printf("%s  %4zu: %-14s R%-3u #%-3u %s\n",pad.c_str(),addr,"ENUM_CREATE",a,bx,
            (bx<consts.size())?fmt(consts[bx],strs).c_str():"?"); break;
        case 75: printf("%s  %4zu: ENUM_VARIANT  R%u  #%u  #%u\n",pad.c_str(),addr,a,b,c); break;
        case 76: printf("%s  %4zu: ENUM_DATA_VAR R%u  #%u\n",pad.c_str(),addr,a,b); break;
        case 77: printf("%s  %4zu: ENUM_GET      R%u = R%u.%s\n",pad.c_str(),addr,a,b,
            (c<consts.size())?fmt(consts[c],strs).c_str():"?"); break;
        case 78: printf("%s  %4zu: ENUM_IS       R%u = is_enum(R%u, %s)\n",pad.c_str(),addr,a,b,
            (c<consts.size())?fmt(consts[c],strs).c_str():"?"); break;
        default: printf("%s  %4zu: ???(%u) R%u R%u R%u\n",pad.c_str(),addr,op,a,b,c); break;
        }
    }

    // Disassemble nested functions
    for(size_t i=0;i<consts.size();i++) {
        if(consts[i].type==CFunc) {
            printf("\n");
            disasm("fn "+consts[i].fname+" arity="+std::to_string(consts[i].farity)+
                   " regs="+std::to_string(consts[i].fregs),
                   consts[i].fbc, consts[i].fconsts, strs, depth+1);
        }
    }
}

int main(int argc, char** argv) {
    if(argc<2){fprintf(stderr,"Usage: akar_disasm <file.ako>\n");return 1;}

    std::ifstream file(argv[1],std::ios::binary);
    if(!file.is_open()){fprintf(stderr,"Error: cannot open '%s'\n",argv[1]);return 1;}

    file.seekg(0,std::ios::end);
    size_t fsz=file.tellg();
    file.seekg(0,std::ios::beg);
    std::vector<uint8_t> data(fsz);
    file.read((char*)data.data(),fsz);
    file.close();

    if(fsz<12){fprintf(stderr,"Error: file too small\n");return 1;}

    // Header: native endian struct {uint32 magic, uint16 ver, uint16 flags, uint32 sec_count}
    uint32_t magic, sec_count;
    uint16_t version, flags;
    std::memcpy(&magic, data.data(), 4);
    std::memcpy(&version, data.data()+4, 2);
    std::memcpy(&flags, data.data()+6, 2);
    std::memcpy(&sec_count, data.data()+8, 4);

    if(magic!=0x414B4152){fprintf(stderr,"Error: not .ako (magic=0x%08x)\n",magic);return 1;}
    if(version!=2){fprintf(stderr,"Error: version %u\n",version);return 1;}

    printf("=== Akar Disassembler ===\n");
    printf("File: %s (%zu bytes)\n",argv[1],fsz);
    printf("Version: %u  Flags: 0x%04x%s\n",version,flags,(flags&1)?" (debug)":" (stripped)");
    printf("Sections: %u\n\n",sec_count);

    // Section headers: {uint8 type, 3 pad, uint32 offset, uint32 size} = 12 bytes each
    struct SH { uint8_t type; uint8_t pad[3]; uint32_t offset, size; };
    std::vector<SH> secs(sec_count);
    for(uint32_t i=0;i<sec_count;i++){
        if(12+i*12+12>fsz){fprintf(stderr,"Error: truncated\n");return 1;}
        std::memcpy(&secs[i], data.data()+12+i*12, 12);
    }

    auto find=[&](uint32_t t)->const SH*{for(auto&s:secs)if(s.type==t)return &s;return nullptr;};

    for(auto&s:secs){
        const char* nm="?";
        if(s.type==1)nm="STRINGS";else if(s.type==2)nm="CONSTANTS";
        else if(s.type==3)nm="BYTECODE";else if(s.type==4)nm="FUNCTIONS";
        else if(s.type==5)nm="ENTRY";
        printf("  [%u] %-10s offset=%-6u size=%u\n",s.type,nm,s.offset,s.size);
    }
    printf("\n");

    // Read strings (big-endian encoded section data)
    auto* ss=find(1);
    std::vector<std::string> strings;
    if(ss && ss->size>0){
        size_t p=ss->offset;
        uint32_t cnt=ru32(data.data(),p);
        printf("Strings (%u):\n",cnt);
        for(uint32_t i=0;i<cnt;i++){
            uint32_t len=ru32(data.data(),p);
            std::string s((const char*)(data.data()+p),len); p+=len;
            strings.push_back(s);
            printf("  [%u] \"%s\"\n",i,s.c_str());
        }
        printf("\n");
    }

    // Read constants
    auto* cs=find(2);
    std::vector<Const> constants;
    if(cs && cs->size>0){
        size_t p=cs->offset;
        uint32_t cnt=ru32(data.data(),p);
        for(uint32_t i=0;i<cnt;i++) constants.push_back(rc(data.data(),p));
    }

    // Read function metadata
    auto* fs=find(4);
    std::string fn="<entry>"; uint16_t fa=0,fr=0; bool fv=false;
    if(fs && fs->size>0){
        size_t p=fs->offset;
        fn=rstr(data.data(),p); fa=ru16(data.data(),p); fr=ru16(data.data(),p);
        fv=ru8(data.data(),p)!=0;
    }

    printf("Entry: %s (arity=%u, regs=%u, varargs=%s)\n\n",fn.c_str(),fa,fr,fv?"true":"false");

    // Read bytecode
    auto* bs=find(3);
    std::vector<uint8_t> bc;
    if(bs && bs->size>0) bc.assign(data.data()+bs->offset, data.data()+bs->offset+bs->size);

    disasm(fn, bc, constants, strings, 0);
    return 0;
}
