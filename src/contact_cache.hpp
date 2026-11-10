// Cached items contain candidate indices and interpolation weights only.
// The identity depends on geometry contents, placement and the actual search
// structures built under the current face/edge budgets, never on RW pointers.
using Stamp=std::array<uint64_t,2>;
static void fingerprint(Stamp& stamp,const void* data,size_t length){
 const auto* p=static_cast<const unsigned char*>(data);
 // All production inputs are arrays of 32-bit words (including pairs of
 // 16-bit triangle indices). memcpy also handles unaligned RW arrays.
 while(length>=4){
  uint32_t word;memcpy(&word,p,4);p+=4;length-=4;
  stamp[0]=(stamp[0]^word)*1099511628211ull;
  stamp[1]+=word;stamp[1]+=stamp[1]<<10;stamp[1]^=stamp[1]>>6;
 }
 while(length--){
  stamp[0]=(stamp[0]^*p)*1099511628211ull;
  stamp[1]+=*p++;stamp[1]+=stamp[1]<<10;stamp[1]^=stamp[1]>>6;
 }
}
struct ContactKey{
 Stamp owner,neighbor;
 bool internal,faces;
 bool operator<(const ContactKey& o)const{
  return std::tie(owner,neighbor,internal,faces)<std::tie(o.owner,o.neighbor,o.internal,o.faces);
 }
};
class ContactCache{
 using Rows=std::vector<Patch>;
 std::map<ContactKey,Rows> entries;
 std::deque<ContactKey> order;
 size_t bytes=0;
public:
 const Rows* find(const ContactKey& k)const{
  auto it=entries.find(k);return it==entries.end()?nullptr:&it->second;
 }
 void save(const ContactKey& k,Rows rows){
  if(entries.count(k))return;
  size_t add=rows.capacity()*sizeof(Patch);
  while(!order.empty()&&(entries.size()>=8192 || bytes+add>12*1024*1024)){
   auto it=entries.find(order.front());
   bytes-=it->second.capacity()*sizeof(Patch);
   entries.erase(it);order.pop_front();
  }
  bytes+=add;entries.emplace(k,std::move(rows));order.push_back(k);
 }
 void clear(){entries.clear();order.clear();bytes=0;}
};
static ContactCache contacts;
