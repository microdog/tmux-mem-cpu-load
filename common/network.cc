/* vim: tabstop=2 shiftwidth=2 expandtab textwidth=80 linebreak wrap
 *
 * Copyright 2012 Matthew McCormick
 * Copyright 2015 Pawel 'l0ner' Soltys
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sstream>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "network.h"
#include "luts.h"
#include "powerline.h"

std::string format_guid_string( unsigned int data1,
  unsigned short data2,
  unsigned short data3,
  const unsigned char data4[8] )
{
  std::ostringstream oss;

  oss << std::hex << std::nouppercase << std::setfill( '0' );
  oss << std::setw( 8 ) << data1;
  oss << "-";
  oss << std::setw( 4 ) << data2;
  oss << "-";
  oss << std::setw( 4 ) << data3;
  oss << "-";
  oss << std::setw( 2 ) << static_cast< unsigned int >( data4[0] );
  oss << std::setw( 2 ) << static_cast< unsigned int >( data4[1] );
  oss << "-";
  for( int i = 2; i < 8; ++i )
  {
    oss << std::setw( 2 ) << static_cast< unsigned int >( data4[i] );
  }

  return oss.str();
}

std::string format_rate( float rate )
{
  std::ostringstream oss;
  oss.precision( 1 );
  oss.setf( std::ios::fixed );
  oss.width( 3 );
  oss.fill( ' ' );
  
  if( rate < 1024 )
  {
    oss << std::right << static_cast<int>(rate) << "B/s";
  }
  else if( rate < 1024 * 1024 )
  {
    float kb_rate = rate / 1024.0f;
    if( kb_rate >= 10.0f )
      oss.precision( 0 );
    oss << std::right << kb_rate << "KB/s";
  }
  else if( rate < 1024 * 1024 * 1024 )
  {
    float mb_rate = rate / (1024.0f * 1024.0f);
    if( mb_rate >= 10.0f )
      oss.precision( 0 );
    oss << std::right << mb_rate << "MB/s";
  }
  else
  {
    float gb_rate = rate / (1024.0f * 1024.0f * 1024.0f);
    if( gb_rate >= 10.0f )
      oss.precision( 0 );
    oss << std::right << gb_rate << "GB/s";
  }
  
  return oss.str();
}

unsigned int get_net_color_idx( const NetworkStatus & net_status )
{
  float max_rate = std::max( net_status.rx_rate, net_status.tx_rate );
  unsigned int color_idx = 0;
  
  if( max_rate > 0 )
  {
    if( max_rate < 100 * 1024 )
    {
      color_idx = static_cast<unsigned int>( (max_rate / (100 * 1024)) * 33 );
    }
    else if( max_rate < 10 * 1024 * 1024 )
    {
      float normalized = (max_rate - 100 * 1024) / (10 * 1024 * 1024 - 100 * 1024);
      color_idx = 33 + static_cast<unsigned int>( normalized * 33 );
    }
    else
    {
      float normalized = std::min( max_rate / (100 * 1024 * 1024), 1.0f );
      color_idx = 66 + static_cast<unsigned int>( normalized * 34 );
    }
  }
  
  return std::min( color_idx, 100u );
}

std::string net_string( const NetworkStatus & net_status,
  NETWORK_MODE mode,
  bool use_colors,
  bool use_powerline_left,
  bool use_powerline_right,
  bool segments_to_right,
  short right_color )
{
  (void) use_powerline_left;
  (void) segments_to_right;
  (void) right_color;

  if( mode == NETWORK_MODE_OFF || net_status.state == NETWORK_STATE_HIDDEN )
  {
    return "";
  }

  std::ostringstream oss;
  unsigned int color_idx = get_net_color_idx( net_status );

  if( use_colors )
  {
    if( use_powerline_right )
    {
      oss << "#[bg=default]";
      powerline( oss, net_lut[color_idx], POWERLINE_RIGHT );
    }
    else if( use_powerline_left )
    {
      powerline( oss, net_lut[color_idx], NONE );
    }
    else
    {
      powerline( oss, net_lut[color_idx], NONE );
    }
  }

  std::ostringstream content_oss;
  content_oss << " ";

  if( net_status.state == NETWORK_STATE_ERROR )
  {
    content_oss << "!net";
  }
  else
  {
    switch( mode )
    {
      case NETWORK_MODE_DOWNLOAD_ONLY:
        content_oss << "↓" << format_rate( net_status.rx_rate );
        break;

      case NETWORK_MODE_UPLOAD_ONLY:
        content_oss << "↑" << format_rate( net_status.tx_rate );
        break;

      case NETWORK_MODE_DYNAMIC:
        if( net_status.rx_rate >= net_status.tx_rate )
        {
          content_oss << "↓" << format_rate( net_status.rx_rate );
        }
        else
        {
          content_oss << "↑" << format_rate( net_status.tx_rate );
        }
        break;

      case NETWORK_MODE_DEFAULT:
      default:
        content_oss << "↓" << format_rate( net_status.rx_rate );
        content_oss << " ↑" << format_rate( net_status.tx_rate );
        break;
    }
  }

  std::string content = content_oss.str();
  size_t expected_width = ( net_status.state == NETWORK_STATE_ERROR ) ?
    5 :
    ( mode == NETWORK_MODE_DEFAULT ? 19 : 9 );

  if( content.length() < expected_width )
  {
    content.append( expected_width - content.length(), ' ' );
  }

  oss << content;
  return oss.str();
}
