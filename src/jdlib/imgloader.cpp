// ライセンス: GPL2

//#define _DEBUG
//#define _VERBOSE
#include "jddebug.h"
#include "gtkmmversion.h"

#include "imgloader.h"
#include "cache.h"

#include "config/globalconf.h"

#include <fstream>


using namespace JDLIB;

/**********************************************************/
/* constructions ******************************************/

ImgLoader::ImgLoader( const std::string& file )
    : m_file( file )
    , m_loadedlevel( LOADLEVEL_INIT )
{
#ifdef _DEBUG
    std::cout << "ImgLoader::ImgLoader file = " << m_file << std::endl;
#endif
}


ImgLoader::~ImgLoader()
{
#ifdef _DEBUG
    std::cout << "ImgLoader::~ImgLoader file = " << m_file << std::endl;
#endif
}


// static
// いつまでもImgLoaderを保持していると、キャッシュアウトしても解放されないため、
// ImgLoaderは不要になった時点で、ただちに解放(.clear()または他の値代入)すること
Glib::RefPtr< ImgLoader > ImgLoader::get_loader( const std::string& file )
{
    ImgProvider& provider = ImgProvider::get_provider();
    std::lock_guard< std::mutex > lock( provider.m_provider_lock );
    Glib::RefPtr< ImgLoader > loader = provider.get_loader( file );
    if( ! loader ) {
        loader = Glib::RefPtr< ImgLoader >( new ImgLoader( file ) );
        provider.set_loader( loader );
    }
    return loader;
}

/**********************************************************/
/* external interface to PixbufLoader *********************/

// 画像サイズ取得
bool ImgLoader::get_size( int& width, int& height )
{
    std::lock_guard< std::mutex > lock( m_loader_lock );
    bool ret = load_imgfile( LOADLEVEL_SIZEONLY );
    width = m_width;
    height = m_height;
    return ret;
}

Glib::RefPtr< Gdk::Pixbuf > ImgLoader::get_pixbuf( const bool pixbufonly )
{
    Glib::RefPtr< Gdk::Pixbuf > ret;
    std::lock_guard< std::mutex > lock( m_loader_lock );
    if( load_imgfile( pixbufonly ? LOADLEVEL_PIXBUFONLY : LOADLEVEL_NORMAL ) ) {
        ret = m_loader->get_pixbuf();
    }
    return ret;
}

Glib::RefPtr< Gdk::PixbufAnimation > ImgLoader::get_animation()
{
    Glib::RefPtr< Gdk::PixbufAnimation > ret;
    std::lock_guard< std::mutex > lock( m_loader_lock );
    if( load_imgfile( LOADLEVEL_NORMAL ) ) {
        ret = m_loader->get_animation();
    }
    return ret;
}

// 画像読み込み
// 動画でpixbufonly = true の時はアニメーションさせない
bool ImgLoader::load( const bool pixbufonly )
{
    std::lock_guard< std::mutex > lock( m_loader_lock );
    return load_imgfile( pixbufonly ? LOADLEVEL_PIXBUFONLY : LOADLEVEL_NORMAL );
}

/**********************************************************/
/* create PixbufLoader ************************************/

// private, NOT thread safe
bool ImgLoader::load_imgfile( const int loadlevel )
{
    if( m_loader ) {
        // キャッシュに読み込んだデータが十分かどうか
        if( m_loadedlevel <= loadlevel ) {
#ifdef _DEBUG
            std::cout << "ImgLoader use cache loadlevel / loadedlevel = " << loadlevel << " / " << m_loadedlevel
                      << " file = " << m_file << std::endl;
#endif
            return true;
        }
        // リロード
        m_width = 0;
        m_height = 0;
        m_stop = false;
        m_y = 0;
    }

    m_loadlevel = loadlevel;

#ifdef _DEBUG
    std::cout << "ImgLoader::load_imgfile start loadlevel = " << loadlevel
              << " loadedlevel = " << m_loadedlevel
              << " file = " << m_file << std::endl;
    size_t total = 0;
#endif

    bool ret = true;

    std::ifstream f( to_locale_cstr( m_file ), std::ios_base::binary );
    if( ! f.is_open() ) {
        m_errmsg = "cannot file open";
        return false;
    }

    try {
        m_loader = Gdk::PixbufLoader::create();

        m_loader->signal_size_prepared().connect( sigc::mem_fun( *this, &ImgLoader::slot_size_prepared ) );
        m_loader->signal_area_updated().connect( sigc::mem_fun( *this, &ImgLoader::slot_area_updated ) );

        constexpr std::size_t bufsize = 8192;
        guint8 data[ bufsize ];

        while( ! m_stop && f ) {

            f.read( reinterpret_cast<char*>(data), bufsize );
            const auto readsize = f.gcount();
            if( readsize > 0 ) m_loader->write( data, readsize );

#ifdef _DEBUG
            total += readsize;
//            std::cout << readsize << " / " << total << std::endl;
#endif
            if( f.eof() ) { // 画像データ全体を読み込み完了
                m_loadedlevel = LOADLEVEL_NORMAL;
                break;
            }
        }

        m_loader->close();
    }
    catch( Glib::Error& err )
    {
#ifdef _DEBUG
        std::string stop_s = m_stop ? "true" : "false";
        std::cout << "ImgLoader stop = (" << stop_s << ") : " << m_file << std::endl;
#endif
        if( ! m_stop ){
            m_errmsg = err.what();
            m_loader.reset();
            ret = false;
        }
    }

    f.close();

#ifdef _DEBUG
    std::cout << "ImgLoader::load_imgfile fisished read = " << total << "  w = " << m_width << " h = " << m_height
              << " loadedlevel = " << m_loadedlevel << std::endl;
#endif

    return ret;
}


// PixbufLoaderが生成する画像の大きさを計算するのに必要な量のデータを受け取った時のシグナルハンドラ
void ImgLoader::slot_size_prepared( int w, int h )
{
#ifdef _DEBUG
    std::cout << "ImgLoader::slot_size_prepared w = " << w << " h = " << h << std::endl;
#endif

    m_width = w;
    m_height = h;
    if( m_loadedlevel > LOADLEVEL_SIZEONLY ) m_loadedlevel = LOADLEVEL_SIZEONLY;
    if( m_loadlevel >= LOADLEVEL_SIZEONLY ) request_stop();
}


// 画像の一部分が更新された時のシグナルハンドラ
void ImgLoader::slot_area_updated(int x, int y, int w, int h )
{
    if( m_loadlevel >= LOADLEVEL_PIXBUFONLY ){

#if defined( _DEBUG ) && defined( _VERBOSE )
        std::cout << "ImgLoader::slot_area_updated x = " << x << " y = " << y << " w = " << w << " h = " << h << std::endl;
#endif

        // アニメーション画像を表示する際、幅や高さが元の値と異なる時に、全ての画像データを
        // 読み込まなくても pixbuf だけ取り出せれば良いので、pixbufを取り出せるようになった時点で
        // 画像データの読み込みを途中で止めて表示待ち時間を短縮する
        if( y < m_y ){
            m_loadedlevel = LOADLEVEL_PIXBUFONLY;
            request_stop();
        }
        m_y = y;
    }
}

/**********************************************************/
/* interface inner behavior *******************************/

// 読み込み中断のリクエスト
void ImgLoader::request_stop()
{
    // 中断をリクエストされても実際には読み込みが完了していることがある
    m_stop = true;
}

bool ImgLoader::equals( const std::string& file ) const
{
    return m_file == file;
}

/**********************************************************/
/* ImgProvider has relational <use> from ImgLoader ********/

ImgProvider::ImgProvider()
{
}

// static
ImgProvider& ImgProvider::get_provider()
{
    // singleton provider
    static ImgProvider instance;
    return instance;
}

// NOT thread safe
Glib::RefPtr< ImgLoader > ImgProvider::get_loader( const std::string& file )
{
    // ImgLoaderキャッシュをサーチ
    auto it = std::find_if( m_cache.begin(), m_cache.end(),
                            [&file]( const Glib::RefPtr<ImgLoader>& loader ) { return loader->equals( file ); } );
    if( it != m_cache.begin() && it != m_cache.end() ) {
        Glib::RefPtr<ImgLoader> ret = *it;
        // LRU: キャッシュをlistの先頭に移動
        m_cache.erase( it );
        m_cache.push_front( ret );
        return ret;
    }
    return Glib::RefPtr< ImgLoader >(); //null
}

// NOT thread safe
void ImgProvider::set_loader( Glib::RefPtr< ImgLoader > loader )
{
    const int size = CONFIG::get_imgcache_size();
    if( size > 0 ) {
        if( m_cache.size() >= static_cast<std::size_t>(size) ) {
            m_cache.pop_back();
        }
        m_cache.push_front( loader );
    }
}


