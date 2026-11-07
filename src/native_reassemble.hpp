static IDirect3DVertexShader9*NativeReassemble(IDirect3DDevice9*d,const std::vector<uint32_t>&tokens){
 HMODULE module=LoadLibraryExW(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!module)return nullptr;
 using Assemble=HRESULT(WINAPI*)(const void*,SIZE_T,const char*,const D3D_SHADER_MACRO*,ID3DInclude*,UINT,ID3DBlob**,ID3DBlob**);
 decltype(&D3DDisassemble) disassemble=nullptr;Assemble assemble=nullptr;
 auto dp=GetProcAddress(module,"D3DDisassemble"),ap=GetProcAddress(module,"D3DAssemble");
 memcpy(&disassemble,&dp,sizeof(dp));memcpy(&assemble,&ap,sizeof(ap));
 ID3DBlob*text=nullptr,*code=nullptr,*error=nullptr;IDirect3DVertexShader9*shader=nullptr;
 if(disassemble&&assemble&&SUCCEEDED(disassemble(tokens.data(),tokens.size()*4,0,nullptr,&text))&&text){
  size_t n=text->GetBufferSize();auto*p=static_cast<const char*>(text->GetBufferPointer());while(n&&!p[n-1])--n;
  HRESULT hr=assemble(p,n,"PSXFX.SA_native",nullptr,nullptr,0,&code,&error);
  if(SUCCEEDED(hr)&&code)d->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()),&shader);
 }
 if(error)error->Release();
 if(code)code->Release();
 if(text)text->Release();
 FreeLibrary(module);return shader;
}
