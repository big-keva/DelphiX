# include "src/indexer/dynamic-chains-ringbuffer.hpp"
# include <mtc/test-it-easy.hpp>
# include <chrono>
# include <thread>

using namespace DelphiX::indexer::dynamic;

TestItEasy::RegisterFunc  dynamic_chains_queue( []()
  {
    TEST_CASE( "index/dynamic-chains-ringbuffer" )
    {
      SECTION( "RingBuffer may be filled at max N - 1 elements, if more, it hangs up" )
      {
        RingBuffer<void*, 4>  ringBuffer;
        void*                 localValue;

        SECTION( "initially the buffer is empty" )
        {
          REQUIRE( ringBuffer.Get( localValue ) == false );
        }
        SECTION( "elements may be pushed to the buffer" )
        {
          ringBuffer.Put( (void*)1 );

          SECTION( "pushed elements may be popped" )
          {
            if ( REQUIRE( ringBuffer.Get( localValue ) ) )
            {
              REQUIRE( localValue == (void*)1 );
              REQUIRE( ringBuffer.Get( localValue ) == false );
            }
          }
        }
        SECTION( "multiple elements may be pushed" )
        {
          ringBuffer.Put( (void*)4 );
          ringBuffer.Put( (void*)3 );
          ringBuffer.Put( (void*)2 );

          SECTION( "pushed elements may be popped" )
          {
            if ( REQUIRE( ringBuffer.Get( localValue ) ) )
              REQUIRE( localValue == (void*)4 );
            if ( REQUIRE( ringBuffer.Get( localValue ) ) )
              REQUIRE( localValue == (void*)3 );
            if ( REQUIRE( ringBuffer.Get( localValue ) ) )
              REQUIRE( localValue == (void*)2 );
            REQUIRE( ringBuffer.Get( localValue ) == false );
          }
        }
        SECTION( "adding N elements blocks thread" )
        {
          auto  startTimer = std::chrono::steady_clock::now();

          std::thread( [&]()
            {
              std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
              ringBuffer.Get( localValue );
            } ).detach();

          SECTION( "locked for 500 milliseconds..." )
          {
            ringBuffer.Put( (void*)1 );
            ringBuffer.Put( (void*)2 );
            ringBuffer.Put( (void*)3 );
            ringBuffer.Put( (void*)4 );
          }
          SECTION( "...and unlocked by parallel thread after time" )
          {
            REQUIRE( ringBuffer.Get( localValue ) );
            REQUIRE( std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - startTimer ).count() >= 500 );

            while ( ringBuffer.Get( localValue ) )
              (void)NULL;
          }
        }
      }
    }
  } );
