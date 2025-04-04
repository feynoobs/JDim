// ライセンス: GPL2

//
// 2ch型ノードツリー
//

#ifndef _NODETREE2ch_H
#define _NODETREE2ch_H

#include "nodetree2chcompati.h"
#include <ctime>

namespace DBTREE
{
    class NodeTree2ch : public NodeTree2chCompati
    {
        std::string m_org_url;  // 移転前のオリジナルURL
        [[maybe_unused]] std::time_t m_since_time; // スレが立った時刻
        int m_mode; // 読み込みモード
        int m_res_number_max; // 最大レス数
        std::size_t m_dat_volume_max{}; // 最大DATサイズ(KB)

      public:

        NodeTree2ch( const std::string& url, const std::string& org_url,
                     const std::string& date_modified, time_t since_time );
        ~NodeTree2ch() override;

        int get_res_number_max() const noexcept override { return m_res_number_max; }
        std::size_t get_dat_volume_max() const noexcept override { return m_dat_volume_max; }

      protected:

        void create_loaderdata( JDLIB::LOADERDATA& data ) override;

        void parse_extattr( std::string_view str ) override;

      private:

        void receive_finish() override;
    };
}

#endif
