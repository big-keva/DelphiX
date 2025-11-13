#include <contents.hpp>
#include <text-api.hpp>
#include <vector>
#include <context/pack-format.hpp>
#include <context/pack-images.hpp>
#include <context/processor.hpp>
#include <context/text-image.hpp>
#include <mtc/arena.hpp>
#include <mtc/interfaces.h>
#include <textAPI/DOM-text.hpp>

auto  LoadText( const std::string& path ) -> DelphiX::textAPI::Document
{
  auto  getdoc = DelphiX::textAPI::Document();
  auto  infile = fopen( path.c_str(), "rt" );

  if ( infile != nullptr )
  {
    char  szline[0x1000];

    while ( fgets( szline, sizeof(szline) - 1, infile ) != nullptr )
      getdoc.AddString( szline );

    fclose( infile );
  }
  return getdoc;
}

int   main()
{
  auto  mArena = mtc::Arena();
  auto  pwBody = mArena.Create<DelphiX::context::BaseImage<mtc::Arena::allocator<char>>>();
  auto  inText = LoadText( "/home/keva/dev/origin/dev/baalbek-dev/tests/queries/mailGenerator.cpp" );
  auto  utfdoc = mArena.Create<DelphiX::textAPI::BaseDocument<mtc::Arena::allocator<char>>>();
    CopyUtf16( utfdoc, inText );
  auto  lgproc = DelphiX::context::Processor();

  lgproc.WordBreak( *pwBody, *utfdoc );
  lgproc.SetMarkup( *pwBody, *utfdoc );
  lgproc.Lemmatize( *pwBody );

  auto  limage = DelphiX::context::imaging::Pack( pwBody->GetTokens() );

  fprintf( stdout, "%u\n", limage.size() );
  
  auto  uimage = DelphiX::context::imaging::Unpack( limage );

  return 0;
}