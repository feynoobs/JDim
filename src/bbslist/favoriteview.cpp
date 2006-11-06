// ライセンス: 最新のGPL

//#define _DEBUG
#include "jddebug.h"

#include "favoriteview.h"
#include "bbslistadmin.h"
#include "selectdialog.h"

#include "cache.h"
#include "sharedbuffer.h"

using namespace BBSLIST;


FavoriteListView::FavoriteListView( const std::string& url,
                                    const std::string& arg1, const std::string& arg2 )
    : BBSListViewBase( url, arg1, arg2 )
{
    // D&D可
    get_treeview().set_reorderable_view( true );
}


FavoriteListView::~FavoriteListView()
{
#ifdef _DEBUG    
    std::cout << "FavoriteList::~FavoriteList : " << get_url() << std::endl;
#endif 

    save_xml( CACHE::path_xml_favorite() );
}


void FavoriteListView::shutdown()
{
#ifdef _DEBUG    
    std::cout << "FavoriteList::shutdown\n";
#endif
    save_xml( CACHE::path_xml_favorite_bkup() );
}


//
// コマンド
//
bool FavoriteListView::set_command( const std::string& command, const std::string& arg )
{
    if( command == "append_favorite" ) append_favorite();
    if( command == "save_favorite" ) save_xml( CACHE::path_xml_favorite() );

    return true;
}



//
// 表示
//
void FavoriteListView::show_view()
{
    BBSLIST::get_admin()->set_command( "set_tablabel", get_url(), "お気に入り" );

    std::string xml;
    CACHE::load_rawdata( CACHE::path_xml_favorite() , xml );
    xml2tree( xml );
    update_urls();
}


//
// アップデート
//
void FavoriteListView::update_view()
{
    update_urls();
}



//
// ポップアップメニュー取得
//
// SKELETON::View::show_popupmenu() を参照すること
//
Gtk::Menu* FavoriteListView::get_popupmenu( const std::string& url )
{
    Gtk::Menu* popupmenu;
    if( url.empty() ) popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu_favorite_space" ) );
    else{
        std::list< Gtk::TreeModel::iterator > list_it = get_treeview().get_selected_iterators();
        if( list_it.size() == 1 ) popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu_favorite" ) );
        else popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu_favorite_mul" ) );
    }

    return popupmenu;
}


//
// お気に入りにアイテム追加
//
// あらかじめ共有バッファにデータを入れておくこと
//
void  FavoriteListView::append_favorite()
{
    if( CORE::SBUF_size() == 0 ) return;
    
    SelectListDialog diag( get_url(), get_treestore() );
    if( diag.run() != Gtk::RESPONSE_OK ) return;
    BBSListViewBase::append_from_buffer( diag.get_path(), true, true );
}


//
// お気に入り保存
//
void FavoriteListView::save_xml( const std::string& file )
{
    if( get_ready_tree() ) CACHE::save_rawdata( file , tree2xml() );
}
