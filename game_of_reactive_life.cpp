#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#include <curses.h>

#include "memorystat.h"
#include "ureact/ureact.hpp"

static void finish( int sig );

class GameBoard
{
public:
    GameBoard( ureact::context& ctx, int width, int height, const std::vector<bool>& values )
        : m_ctx( ctx )
        , m_width( width )
        , m_height( height )
    {
        const int fields = width * height;
        assert( values.size() == size_t( fields ) );

        m_oldBoard.reserve( fields );
        for( int i = 0; i < fields; ++i )
        {
            auto var = ctx.make_var( values[i] );
            m_oldBoard.push_back( std::move( var ) );
        }

        auto oldBoardFieldByPos = [&]( std::pair<int, int> pos ) {
            const auto posWrapped = wrapPos( pos );
            const int i = posToFieldId( posWrapped );
            return m_oldBoard[i];
        };

        // clang-format off
        auto updateField = [this]( bool self, bool tl, bool t, bool tr, bool l, bool r, bool bl, bool b, bool br )
        {
            // clang-format on
            ++m_recalculated;
            const int neighbours = tl + t + tr + l + r + bl + b + br;
            if( self )
            {
                // If a cell is ON and has only 1 neighbour then it turns OFF in the next turn. (SOLITUDE)
                // If a cell is ON and has 2 or 3 neighbours then it remains ON in the next turn.
                // If a cell is ON and has 4 or more neighbours then it turns OFF in the next turn. (OVERPOPULATION)
                return neighbours == 2 || neighbours == 3;
            }
            else
            {
                // If a cell is OFF and has exactly 3 neighbours then it turns ON in the next turn. (REPRODUCTION)
                return neighbours == 3;
            }
        };

        m_newBoard.resize( fields );
        for( int i = 0; i < fields; ++i )
        {
            // clang-format off
            auto self_pos = fieldIdToPos( i );
            auto self = oldBoardFieldByPos( self_pos );
            auto tl   = oldBoardFieldByPos( { self_pos.first - 1, self_pos.second - 1 } );
            auto t    = oldBoardFieldByPos( { self_pos.first,     self_pos.second - 1 } );
            auto tr   = oldBoardFieldByPos( { self_pos.first + 1, self_pos.second - 1 } );
            auto l    = oldBoardFieldByPos( { self_pos.first - 1, self_pos.second     } );
            auto r    = oldBoardFieldByPos( { self_pos.first + 1, self_pos.second     } );
            auto bl   = oldBoardFieldByPos( { self_pos.first - 1, self_pos.second + 1 } );
            auto b    = oldBoardFieldByPos( { self_pos.first,     self_pos.second + 1 } );
            auto br   = oldBoardFieldByPos( { self_pos.first + 1, self_pos.second + 1 } );
            m_newBoard[i] = with(self, tl, t, tr, l, r, bl, b, br) ->* updateField;
            // clang-format on
        }
    }

    void update()
    {
        MEMORYSTAT_SCOPE();
        
        m_recalculated = 0;
        m_ctx.do_transaction( [&]() {
            const int fields = m_width * m_height;
            for( int i = 0; i < fields; ++i )
            {
                m_oldBoard[i] <<= m_newBoard[i].value();
            }
        } );
        
        m_stat = Memory::Instance().GetStat();
    }

    void draw( int yStart, int xStart )
    {
        for( int y = 0; y < m_height; ++y )
        {
            for( int x = 0; x < m_width; ++x )
            {
                const int i = posToFieldId( { x, y } );
                mvprintw( yStart + y, xStart + x, "%c", m_newBoard[i].value() ? 'O' : '.' );
            }
        }
    }

    int recalculated() const
    {
        return m_recalculated;
    }

    bool finished() const
    {
        return m_recalculated == 0;
    }

    const MemoryStat& getStat() { return m_stat; }
    
private:
    std::pair<int, int> fieldIdToPos( const int fieldId ) const
    {
        return { fieldId % m_width, fieldId / m_width };
    }

    int posToFieldId( const std::pair<int, int> pos ) const
    {
        return pos.first + pos.second * m_width;
    }

    std::pair<int, int>& wrapPos( std::pair<int, int>& pos ) const
    {
        pos.first = ( pos.first + m_width ) % m_width;
        pos.second = ( pos.second + m_height ) % m_height;
        return pos;
    }

    ureact::context& m_ctx;
    int m_width{};
    int m_height{};
    std::vector<ureact::var_signal<bool>> m_oldBoard{};
    std::vector<ureact::signal<bool>> m_newBoard{};
    int m_recalculated = -1;
    MemoryStat m_stat {};
};

static bool stopped = false;

#define _ false
#define O true

int main()
{
    MemoryStat initialStat;

    {
    ureact::context ctx;
    // clang-format off
    const std::vector<bool> initial = {
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,O,_,O,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,O,O,_,_,_,_,_,O,O,O,_,_,_,_,_,_,_,_,
        _,_,O,_,_,_,_,_,_,_,_,_,_,_,_,_,O,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,O,_,O,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,O,_,O,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,O,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,O,O,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,O,O,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
    };
    // clang-format on

    const int width = 20;
    const int height = 20;

    GameBoard board( ctx, width, height, initial );

    initialStat = Memory::Instance().GetStat();
    
    signal( SIGINT, finish );
    initscr();
    curs_set(0);

    mvprintw( 0, 0, "Dedicated to John Horton Conway\n(26 December 1937 - 11 April 2020)" );

    bool skipUpdate = true;
    int generation = 0;

    do
    {
        if( !skipUpdate )
        {
            board.update();
        }
        skipUpdate = false;

        // clear();
        
        board.draw( 3, 0 );
        const auto& turnStat = board.getStat();
        
        mvprintw( height + 4, 0, "Generation: %d       ", generation );
        mvprintw( height + 5, 0, "Recalculated nodes: %d       ", board.recalculated() );
//        mvprintw( height + 6, 0, "mallocCount:  %zu         ", turnStat.mallocCount);
//        mvprintw( height + 7, 0, "reallocCount: %zu         ", turnStat.reallocCount);
//        mvprintw( height + 8, 0, "freeCount:    %zu         ", turnStat.freeCount);
//        mvprintw( height + 9, 0, "currentSize:  %zu         ", turnStat.currentSize);
//        mvprintw( height + 10,0, "peakSize:     %zu         ", turnStat.peakSize);

        refresh();

        ++generation;
//         std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
    } while( !(board.finished() || stopped) );

    endwin();
    }
    
    printf("Initial:\n");
    printf("mallocCount:  %zu\n", initialStat.mallocCount);
    printf("reallocCount: %zu\n", initialStat.reallocCount);
    printf("freeCount:    %zu\n", initialStat.freeCount);
    printf("currentSize:  %zu\n", initialStat.currentSize);
    printf("peakSize:     %zu\n", initialStat.peakSize);
    printf("\n");
    
    const auto& stat = Memory::Instance().GetStat();
    
    printf("Final:\n");
    printf("mallocCount:  %zu\n", stat.mallocCount);
    printf("reallocCount: %zu\n", stat.reallocCount);
    printf("freeCount:    %zu\n", stat.freeCount);
    printf("currentSize:  %zu\n", stat.currentSize);
    printf("peakSize:     %zu\n", stat.peakSize);
}

static void finish( int sig )
{
    stopped = true;
}
