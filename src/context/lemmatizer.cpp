# include "../../context/lemmatizer.hpp"
# include <mtc/sharedLibrary.hpp>
# include <memory>

namespace DelphiX {
namespace context {

  class MiniModule final: public ILemmatizer
  {
    implement_lifetime_control

  public:
    int   Lemmatize( IWord*, const widechar*, size_t ) override {  return 0;  }
  };

  class DynaModule final: public ILemmatizer
  {
    implement_lifetime_control

  public:
    DynaModule( mtc::SharedLibrary, const char* );

  public:
    int   Lemmatize( IWord*, const widechar*, size_t ) override;

  protected:
    std::shared_ptr<std::pair<mtc::SharedLibrary, mtc::api<ILemmatizer>>>
      holder;

  };

  class MaxiModule final: public ILemmatizer
  {
    implement_lifetime_control

    int   Lemmatize( IWord*, const widechar*, size_t ) override {  return 0;  }
  };

  // DynaModule implementation

  DynaModule::DynaModule( mtc::SharedLibrary libmod, const char* args )
  {
    auto  module = mtc::api<ILemmatizer>();
    auto  fnInit = (CreateLemmatizer)libmod.Find( "CreateLemmatizer" );
    int   nerror;

    if ( (nerror = fnInit( module, args )) != 0 )
      throw std::runtime_error( "Failed to create lemmatizer , error code: " + std::to_string( nerror ) );

    holder = std::make_shared<std::pair<mtc::SharedLibrary, mtc::api<ILemmatizer>>>(
      libmod, module );
  }

  int   DynaModule::Lemmatize( IWord* lemmas, const widechar* pwsstr, size_t cchstr )
  {
    return holder != nullptr ? holder->second->Lemmatize( lemmas, pwsstr, cchstr ) : EFAULT;
  }

  auto  MiniLemmatizer() -> mtc::api<ILemmatizer>
  {
    return new MiniModule();
  }

  auto  LoadLemmatizer( const char* path, const char* args ) -> mtc::api<ILemmatizer>
  {
    auto  module = mtc::SharedLibrary::Load( path );

    return new DynaModule( module, args );
  }

}}
