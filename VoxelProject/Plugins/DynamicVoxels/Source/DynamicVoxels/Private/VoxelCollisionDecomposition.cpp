#include "VoxelCollisionDecomposition.h"
#include "VoxelGrid.h"
#include "Containers/StaticArray.h"

namespace DecompVoxel
{
    using FAxisOrder = TStaticArray<int32, 3>;

    FAxisOrder MakeAxisOrder(int32 A, int32 B, int32 C)
    {
        FAxisOrder order;
        order[0] = A;
        order[1] = B;
        order[2] = C;
        return order;
    }

    TStaticArray<int32, 3> ToGridCoord(const FAxisOrder& order, int32 I0, int32 I1, int32 I2)
    {
        TStaticArray<int32, 3> coord;
        coord[order[0]] = I0;
        coord[order[1]] = I1;
        coord[order[2]] = I2;
        return coord;
    }

    struct FRun
    {
        int32 Start = 0;
        int32 End = 0;
    };

    TArray<FRun> ExtractRuns(const FVoxelGrid& grid, const FAxisOrder& order, int32 N0, int32 I1, int32 I2)
    {
        TArray<FRun> runs;
        bool inRun = false;
        int32 start = 0;
        for (int32 I0 = 0; I0 <= N0; ++I0)
        {
            bool bOccupied = false;
            if (I0 < N0)
            {
                const TStaticArray<int32, 3> coord = ToGridCoord(order, I0, I1, I2);
                bOccupied = grid.IsFilled(coord[0], coord[1], coord[2]);
            }
            if (bOccupied && !inRun)
            {
                start = I0;
                inRun = true;
            }
            else if (!bOccupied && inRun)
            {
                runs.Add(FRun{start, I0});
                inRun = false;
            }
        }
        return runs;
    }

    struct FRunKey
    {
        int32 Start = 0;
        int32 End = 0;
        bool operator==(const FRunKey& other) const { return Start == other.Start && End == other.End; }
    };

    FORCEINLINE uint32 GetTypeHash(const FRunKey& key) { return HashCombine(::GetTypeHash(key.Start), ::GetTypeHash(key.End)); }

    struct FRect2D
    {
        int32 S0 = 0, E0 = 0, S1 = 0, E1 = 0;
    };

    struct FRectKey
    {
        int32 S0 = 0, E0 = 0, S1 = 0, E1 = 0;
        bool operator==(const FRectKey& other) const
        {
            return S0 == other.S0 && E0 == other.E0 && S1 == other.S1 && E1 == other.E1;
        }
    };

    FORCEINLINE uint32 GetTypeHash(const FRectKey& key)
    {
        uint32 hash = ::GetTypeHash(key.S0);
        hash = HashCombine(hash, ::GetTypeHash(key.E0));
        hash = HashCombine(hash, ::GetTypeHash(key.S1));
        hash = HashCombine(hash, ::GetTypeHash(key.E1));
        return hash;
    }

    TArray<TArray<FRect2D>> BuildPlates(const FVoxelGrid& grid, const FAxisOrder& order, int32 N0, int32 N1, int32 N2)
    {
        TArray<TArray<FRect2D>> plates;
        plates.SetNum(N2);
        for (int32 I2 = 0; I2 < N2; ++I2)
        {
            TMap<FRunKey, FRect2D> Active;
            for (int32 I1 = 0; I1 <= N1; ++I1)
            {
                TArray<FRun> runs;
                if (I1 < N1) runs = ExtractRuns(grid, order, N0, I1, I2);
                TSet<FRunKey> MatchedThisRow;
                for (const FRun& Run : runs)
                {
                    const FRunKey Key{Run.Start, Run.End};
                    if (FRect2D* Existing = Active.Find(Key))
                    {
                        Existing->E1 = I1 + 1;
                    }
                    else
                    {
                        Active.Add(Key, FRect2D{Run.Start, Run.End, I1, I1 + 1});
                    }
                    MatchedThisRow.Add(Key);
                }
                for (auto It = Active.CreateIterator(); It; ++It)
                {
                    if (!MatchedThisRow.Contains(It->Key))
                    {
                        plates[I2].Add(It->Value);
                        It.RemoveCurrent();
                    }
                }
            }
        }
        return plates;
    }

    struct FBlock3DInternal
    {
        int32 S0 = 0, E0 = 0, S1 = 0, E1 = 0, S2 = 0, E2 = 0;
    };

    TArray<FBlock3DInternal> MergePlates(const TArray<TArray<FRect2D>>& plates, int32 N2)
    {
        TArray<FBlock3DInternal> result;
        TMap<FRectKey, FBlock3DInternal> active;
        for (int32 I2 = 0; I2 <= N2; ++I2)
        {
            TSet<FRectKey> matchedThisSlice;
            if (I2 < N2)
            {
                for (const FRect2D& Rect : plates[I2])
                {
                    const FRectKey Key{Rect.S0, Rect.E0, Rect.S1, Rect.E1};
                    if (FBlock3DInternal* Existing = active.Find(Key))
                    {
                        Existing->E2 = I2 + 1;
                    }
                    else
                    {
                        active.Add(Key, FBlock3DInternal{Rect.S0, Rect.E0, Rect.S1, Rect.E1, I2, I2 + 1});
                    }
                    matchedThisSlice.Add(Key);
                }
            }
            for (auto It = active.CreateIterator(); It; ++It)
            {
                if (!matchedThisSlice.Contains(It->Key))
                {
                    result.Add(It->Value);
                    It.RemoveCurrent();
                }
            }
        }
        return result;
    }

    TArray<FVoxelBlock> DecomposeWithOrder(const FVoxelGrid& Grid, const FAxisOrder& Order)
    {
        const int32 N = static_cast<int32>(Grid.GridSize);
        const TArray<TArray<FRect2D>> plates = BuildPlates(Grid, Order, N, N, N);
        const TArray<FBlock3DInternal> blocks4D = MergePlates(plates, N);
        TArray<FVoxelBlock> blocks;
        blocks.Reserve(blocks4D.Num());
        for (const FBlock3DInternal& B : blocks4D)
        {
            const TStaticArray<int32, 3> MinCoord = ToGridCoord(Order, B.S0, B.S1, B.S2);
            TStaticArray<int32, 3> Size;
            Size[Order[0]] = B.E0 - B.S0;
            Size[Order[1]] = B.E1 - B.S1;
            Size[Order[2]] = B.E2 - B.S2;
            FVoxelBlock block;
            block.X = MinCoord[0];
            block.Y = MinCoord[1];
            block.Z = MinCoord[2];
            block.SizeX = Size[0];
            block.SizeY = Size[1];
            block.SizeZ = Size[2];
            blocks.Add(block);
        }
        return blocks;
    }

    TArray<FVoxelBlock> Decompose3GDM(const FVoxelGrid& grid)
    {
        static const FAxisOrder orders[6] = {MakeAxisOrder(0, 1, 2),
                                             MakeAxisOrder(0, 2, 1),
                                             MakeAxisOrder(1, 0, 2),
                                             MakeAxisOrder(1, 2, 0),
                                             MakeAxisOrder(2, 0, 1),
                                             MakeAxisOrder(2, 1, 0)};
        TArray<FVoxelBlock> Best;
        int32 BestCount = TNumericLimits<int32>::Max();
        for (const FAxisOrder& order : orders)
        {
            TArray<FVoxelBlock> blocks = DecomposeWithOrder(grid, order);
            if (blocks.Num() < BestCount)
            {
                BestCount = blocks.Num();
                Best = MoveTemp(blocks);
            }
        }
        return Best;
    }

    // 3D summed-area table over a cube of side PaddedSize (>= actual grid size,
    // padded with empty/zero beyond the real grid). Gives O(1) box-occupancy sums.
    class F3DSummedAreaTable
    {   
    public:
        void Build(const FVoxelGrid& grid, int32 inPaddedSize)
        {
            PaddedSize = inPaddedSize;
            const int32 N = (int32)grid.GridSize;
            const int32 S = PaddedSize + 1;
            Data.SetNum(S * S * S);
            for (int32 i = 0; i < Data.Num(); ++i) Data[i] = 0;

            for (int32 z = 0; z < PaddedSize; ++z)
                for (int32 y = 0; y < PaddedSize; ++y)
                    for (int32 x = 0; x < PaddedSize; ++x)
                    {
                        int32 v = (x < N && y < N && z < N && grid.IsFilled(x, y, z)) ? 1 : 0;
                        Set(x + 1,
                            y + 1,
                            z + 1,
                            v + Get(x, y + 1, z + 1) + Get(x + 1, y, z + 1) + Get(x + 1, y + 1, z) - Get(x, y, z + 1) -
                                Get(x, y + 1, z) - Get(x + 1, y, z) + Get(x, y, z));
                    }
        }

        // Sum of occupied voxels in [x0,x1) x [y0,y1) x [z0,z1). Coordinates are
        // clamped to [0,PaddedSize] so callers don't need to worry about bounds.
        int64 BoxSum(int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1) const
        {
            x0 = Clamp(x0);
            y0 = Clamp(y0);
            z0 = Clamp(z0);
            x1 = Clamp(x1);
            y1 = Clamp(y1);
            z1 = Clamp(z1);
            if (x1 <= x0 || y1 <= y0 || z1 <= z0) return 0;
            return Get(x1, y1, z1) - Get(x0, y1, z1) - Get(x1, y0, z1) - Get(x1, y1, z0) + Get(x0, y0, z1) + Get(x0, y1, z0) +
                   Get(x1, y0, z0) - Get(x0, y0, z0);
        }

        int32 Size() const { return PaddedSize; }

    private:
        int32 Clamp(int32 v) const { return v < 0 ? 0 : (v > PaddedSize ? PaddedSize : v); }
        int64 Get(int32 x, int32 y, int32 z) const { return Data[Index(x, y, z)]; }
        void Set(int32 x, int32 y, int32 z, int64 v) { Data[Index(x, y, z)] = v; }
        int32 Index(int32 x, int32 y, int32 z) const
        {
            const int32 S = PaddedSize + 1;
            return (z * S + y) * S + x;
        }

        int32 PaddedSize = 0;
        TArray<int64> Data;
    };

    static bool IsBoxHomogeneous(const F3DSummedAreaTable& SAT, int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1, bool& OutFilled)
    {
        const int64 Volume = int64(x1 - x0) * int64(y1 - y0) * int64(z1 - z0);
        const int64 Sum = SAT.BoxSum(x0, y0, z0, x1, y1, z1);
        if (Sum == 0)
        {
            OutFilled = false;
            return true;
        }
        if (Sum == Volume)
        {
            OutFilled = true;
            return true;
        }
        return false;
    }

    static void OctreeRecurse(const F3DSummedAreaTable& SAT, int32 x0, int32 y0, int32 z0, int32 Size, TArray<FVoxelBlock>& Out)
    {
        bool bFilled;
        if (IsBoxHomogeneous(SAT, x0, y0, z0, x0 + Size, y0 + Size, z0 + Size, bFilled))
        {
            if (bFilled)
            {
                FVoxelBlock B;
                B.X = x0;
                B.Y = y0;
                B.Z = z0;
                B.SizeX = Size;
                B.SizeY = Size;
                B.SizeZ = Size;
                Out.Add(B);
            }
            return;
        }
        const int32 H = Size / 2;
        for (int32 dz = 0; dz < 2; ++dz)
            for (int32 dy = 0; dy < 2; ++dy)
                for (int32 dx = 0; dx < 2; ++dx) OctreeRecurse(SAT, x0 + dx * H, y0 + dy * H, z0 + dz * H, H, Out);
    }

    TArray<FVoxelBlock> DecomposeOctree(const FVoxelGrid& grid)
    {
        const int32 N = (int32)grid.GridSize;
        int32 size = 1;
        while (size < N) size <<= 1;  // next power of two >= N (QTD/OTD requirement)

        F3DSummedAreaTable SAT;
        SAT.Build(grid, size);
        TArray<FVoxelBlock> Out;
        OctreeRecurse(SAT, 0, 0, 0, size, Out);
        return Out;
    }

    static void BSPRecurse(const F3DSummedAreaTable& SAT, int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1, TArray<FVoxelBlock>& Out)
    {
        bool bFilled;
        if (IsBoxHomogeneous(SAT, x0, y0, z0, x1, y1, z1, bFilled))
        {
            if (bFilled)
            {
                FVoxelBlock B;
                B.X = x0;
                B.Y = y0;
                B.Z = z0;
                B.SizeX = x1 - x0;
                B.SizeY = y1 - y0;
                B.SizeZ = z1 - z0;
                Out.Add(B);
            }
            return;
        }

        int32 BestAxis = -1, BestCoord = -1;
        int64 BestCost = -1;

        auto Consider = [&](int32 Axis, int32 Coord, int64 Cost)
        {
            if (BestAxis == -1 || Cost < BestCost)
            {
                BestAxis = Axis;
                BestCoord = Coord;
                BestCost = Cost;
            }
        };

        for (int32 x = x0 + 1; x < x1; ++x)
        {
            const int64 Cost = SAT.BoxSum(x - 1, y0, z0, x, y1, z1) + SAT.BoxSum(x, y0, z0, x + 1, y1, z1);
            Consider(0, x, Cost);
        }
        for (int32 y = y0 + 1; y < y1; ++y)
        {
            const int64 Cost = SAT.BoxSum(x0, y - 1, z0, x1, y, z1) + SAT.BoxSum(x0, y, z0, x1, y + 1, z1);
            Consider(1, y, Cost);
        }
        for (int32 z = z0 + 1; z < z1; ++z)
        {
            const int64 Cost = SAT.BoxSum(x0, y0, z - 1, x1, y1, z) + SAT.BoxSum(x0, y0, z, x1, y1, z + 1);
            Consider(2, z, Cost);
        }

        if (BestAxis == -1)
        {
            // Box can't be split further along any axis (degenerate 1-voxel-thick in
            // all dims) but wasn't homogeneous - shouldn't happen, but fall back safely.
            FVoxelBlock B;
            B.X = x0;
            B.Y = y0;
            B.Z = z0;
            B.SizeX = x1 - x0;
            B.SizeY = y1 - y0;
            B.SizeZ = z1 - z0;
            Out.Add(B);
            return;
        }

        if (BestAxis == 0)
        {
            BSPRecurse(SAT, x0, y0, z0, BestCoord, y1, z1, Out);
            BSPRecurse(SAT, BestCoord, y0, z0, x1, y1, z1, Out);
        }
        else if (BestAxis == 1)
        {
            BSPRecurse(SAT, x0, y0, z0, x1, BestCoord, z1, Out);
            BSPRecurse(SAT, x0, BestCoord, z0, x1, y1, z1, Out);
        }
        else
        {
            BSPRecurse(SAT, x0, y0, z0, x1, y1, BestCoord, Out);
            BSPRecurse(SAT, x0, y0, BestCoord, x1, y1, z1, Out);
        }
    }

    TArray<FVoxelBlock> DecomposeBSP(const FVoxelGrid& Grid)
    {
        const int32 N = (int32)Grid.GridSize;
        F3DSummedAreaTable SAT;
        SAT.Build(Grid, N);

        TArray<FVoxelBlock> Out;
        BSPRecurse(SAT, 0, 0, 0, N, N, N, Out);
        return Out;
    }

    static void OtherAxes(int32 axis, int32& OutB, int32& OutC)
    {
        if (axis == 0)
        {
            OutB = 1;
            OutC = 2;
        }
        else if (axis == 1)
        {
            OutB = 0;
            OutC = 2;
        }
        else
        {
            OutB = 0;
            OutC = 1;
        }
    }

    static void MakeCoord(int32 Axis, int32 A, int32 B, int32 C, int32 otherAxisB, int32 otherAxisC, int32 outXYZ[3])
    {
        outXYZ[Axis] = A;
        outXYZ[otherAxisB] = B;
        outXYZ[otherAxisC] = C;
    }

    struct FConcaveEdge
    {
        int32 Axis = 0;            // axis the edge EXTENDS along
        int32 FixedB = 0;          // grid-line coord on OtherAxes(Axis).first
        int32 FixedC = 0;          // grid-line coord on OtherAxes(Axis).second
        int32 Start = 0, End = 0;  // [Start,End) along Axis
    };

    static TArray<FConcaveEdge> FindConcaveEdges(const FVoxelGrid& grid, int32 N, const TSet<int32> walls[3])
    {
        TArray<FConcaveEdge> edges;

        for (int32 axis = 0; axis < 3; ++axis)
        {
            int32 AxB, AxC;
            OtherAxes(axis, AxB, AxC);

            for (int32 b = 1; b < N; ++b)
            {
                if (walls[AxB].Contains(b)) continue;  // already fully resolved
                for (int32 c = 1; c < N; ++c)
                {
                    if (walls[AxC].Contains(c)) continue;

                    bool bInRun = false;
                    int32 RunStart = 0;

                    for (int32 a = 0; a <= N; ++a)
                    {
                        bool bConcave = false;
                        if (a < N)
                        {
                            int32 Coord[3];
                            int32 Count = 0;
                            MakeCoord(axis, a, b - 1, c - 1, AxB, AxC, Coord);
                            Count += grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                            MakeCoord(axis, a, b, c - 1, AxB, AxC, Coord);
                            Count += grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                            MakeCoord(axis, a, b - 1, c, AxB, AxC, Coord);
                            Count += grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                            MakeCoord(axis, a, b, c, AxB, AxC, Coord);
                            Count += grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                            bConcave = (Count == 3);
                        }

                        const bool bWallHere = (a < N) && (a > 0) && walls[axis].Contains(a);
                        if (bInRun)
                        {
                            if (bConcave && !bWallHere)
                            {
                                continue;  // run extends through 'a'
                            }
                            // flush [RunStart, a)
                            FConcaveEdge e;
                            e.Axis = axis;
                            e.FixedB = b;
                            e.FixedC = c;
                            e.Start = RunStart;
                            e.End = a;
                            edges.Add(e);
                            bInRun = false;
                        }

                        if (bConcave)
                        {
                            RunStart = a;
                            bInRun = true;
                        }
                    }
                }
            }
        }
        return edges;
    }

    struct FSeparatorKey
    {
        int32 Axis = 0, Coord = 0;
        bool operator==(const FSeparatorKey& Other) const { return Axis == Other.Axis && Coord == Other.Coord; }
    };

    FORCEINLINE uint32 GetTypeHash(const FSeparatorKey& K) { return HashCombine(::GetTypeHash(K.Axis), ::GetTypeHash(K.Coord)); }

    struct FSeparatorWeight
    {
        int32 Contained = 0;
        int32 Intersected = 0;
        int32 Weight() const { return Contained - Intersected; }
    };

    static TMap<FSeparatorKey, FSeparatorWeight> ComputeSeparatorWeights(const TArray<FConcaveEdge>& edges, int32 N)
    {
        TMap<FSeparatorKey, FSeparatorWeight> Weights;

        for (const FConcaveEdge& e : edges)
        {
            int32 AxB, AxC;
            OtherAxes(e.Axis, AxB, AxC);
            const FSeparatorKey K1{AxB, e.FixedB};
            const FSeparatorKey K2{AxC, e.FixedC};
            if (FSeparatorWeight* W = Weights.Find(K1))
            {
                W->Contained += 1;
            }
            else
            {
                Weights.Add(K1, FSeparatorWeight{1, 0});
            }
            if (FSeparatorWeight* W = Weights.Find(K2))
            {
                W->Contained += 1;
            }
            else
            {
                Weights.Add(K2, FSeparatorWeight{1, 0});
            }
        }

        TArray<int32> Diff[3];
        for (int32 a = 0; a < 3; ++a)
        {
            Diff[a].SetNum(N + 2);
            for (int32 i = 0; i < Diff[a].Num(); ++i) Diff[a][i] = 0;
        }
        for (const FConcaveEdge& e : edges)
        {
            // Strictly-inside(Start,End) means coordinates Start+1 .. End-1.
            if (e.Start + 1 <= e.End - 1)
            {
                Diff[e.Axis][e.Start + 1] += 1;
                Diff[e.Axis][e.End] -= 1;
            }
        }
        TArray<int32> Prefix[3];
        for (int32 a = 0; a < 3; ++a)
        {
            Prefix[a].SetNum(N + 2);
            int32 Running = 0;
            for (int32 i = 0; i <= N + 1; ++i)
            {
                Running += Diff[a][i];
                Prefix[a][i] = Running;
            }
        }

        for (auto It = Weights.CreateIterator(); It; ++It)
        {
            It.Value().Intersected = Prefix[It.Key().Axis][It.Key().Coord];
        }
        return Weights;
    }

    struct FFootprints
    {
        // Footprint[axisPair] : for axes (0,1)->XY, (0,2)->XZ, (1,2)->YZ
        TArray<uint8> XY, XZ, YZ;
        int32 N = 0;

        void Build(const FVoxelGrid& Grid, int32 InN)
        {
            N = InN;
            XY.SetNum(N * N);
            XZ.SetNum(N * N);
            YZ.SetNum(N * N);
            for (int32 i = 0; i < XY.Num(); ++i) XY[i] = 0;
            for (int32 i = 0; i < XZ.Num(); ++i) XZ[i] = 0;
            for (int32 i = 0; i < YZ.Num(); ++i) YZ[i] = 0;

            for (int32 z = 0; z < N; ++z)
                for (int32 y = 0; y < N; ++y)
                    for (int32 x = 0; x < N; ++x)
                    {
                        if (!Grid.IsFilled(x, y, z)) continue;
                        XY[y * N + x] = 1;
                        XZ[z * N + x] = 1;
                        YZ[z * N + y] = 1;
                    }
        }

        bool AnyAt(int32 axis1, int32 coord1, int32 axis2, int32 coord2) const
        {
            // returns true if there's ANY filled voxel with coordinate Coord1 on
            // Axis1 and Coord2 on Axis2 (any value on the third axis).
            int32 a = axis1, ca = coord1, b = axis2, cb = coord2;
            if (a > b)
            {
                Swap(a, b);
                Swap(ca, cb);
            }
            if (a == 0 && b == 1) return ca >= 0 && ca < N && cb >= 0 && cb < N && XY[cb * N + ca] != 0;
            if (a == 0 && b == 2) return ca >= 0 && ca < N && cb >= 0 && cb < N && XZ[cb * N + ca] != 0;
            return ca >= 0 && ca < N && cb >= 0 && cb < N && YZ[cb * N + ca] != 0;  // (1,2) -> YZ
        }

        bool Intersect(const FSeparatorKey& S1, const FSeparatorKey& S2) const
        {
            // Check the up-to-4 voxel columns straddling both grid lines.
            for (int32 dx : {-1, 0})
                for (int32 dy : {-1, 0})
                {
                    if (AnyAt(S1.Axis, S1.Coord + dx, S2.Axis, S2.Coord + dy)) return true;
                }
            return false;
        }
    };

    // Exact Maximum Independent Set via simple branch-and-bound. NumVertices is
    // expected to be small (the paper itself notes exact MaxIS is only
    // practical when the candidate-separator count is low - see the "may not
    // be feasible" caveat in Section 3.3); callers should fall back to IS(1)
    // above some vertex-count threshold.
    static void MISRecurse(const TArray<TArray<uint8_t>>& adj,
                           TArray<int32>& remaining,
                           TArray<int32>& current,
                           TArray<int32>& best)
    {
        if (current.Num() + remaining.Num() <= best.Num()) return;  // can't beat best
        if (remaining.Num() == 0)
        {
            if (current.Num() > best.Num()) best = current;
            return;
        }

        const int32 V = remaining[0];
        TArray<int32> RestWithoutV;
        for (int32 i = 1; i < remaining.Num(); ++i) RestWithoutV.Add(remaining[i]);

        // Branch: exclude V
        MISRecurse(adj, RestWithoutV, current, best);

        // Branch: include V (drop anything adjacent to it)
        TArray<int32> Compatible;
        for (int32 U : RestWithoutV)
            if (!adj[V][U]) Compatible.Add(U);
        current.Add(V);
        MISRecurse(adj, Compatible, current, best);
        current.Pop();
    }

    static TArray<int32> MaxIndependentSet(int32 numVertices, const TArray<TArray<uint8_t>>& adj)
    {
        TArray<int32> All;
        for (int32 i = 0; i < numVertices; ++i) All.Add(i);
        TArray<int32> Current, Best;
        MISRecurse(adj, All, Current, Best);
        return Best;
    }

    // Simple heuristic: largest single-axis group (always independent, since
    // parallel separators never intersect). This is IS(1) from the paper.
    static TArray<int32> LargestAxisGroup(const TArray<FSeparatorKey>& candidates)
    {
        TArray<int32> groups[3];
        for (int32 i = 0; i < candidates.Num(); ++i) groups[candidates[i].Axis].Add(i);
        int32 bestAxis = 0;
        for (int32 a = 1; a < 3; ++a)
            if (groups[a].Num() > groups[bestAxis].Num()) bestAxis = a;
        return groups[bestAxis];
    }

    // Safety cutoff: above this many same-weight candidates, exact MaxIS search
    // is skipped in favor of IS(1) to avoid combinatorial blow-up in-editor.
    static constexpr int32 MaxExactSeparatorCount = 28;

    static TArray<FVoxelBlock> DecomposeSeparatorBased(const FVoxelGrid& grid, bool useExactMaxIS)
    {
        const int32 N = (int32)grid.GridSize;

        F3DSummedAreaTable SAT;
        SAT.Build(grid, N);
        FFootprints Footprints;
        Footprints.Build(grid, N);

        TSet<int32> walls[3];

        for (int32 SafetyIterations = 0; SafetyIterations < N * 3 + 16; ++SafetyIterations)
        {
            TArray<FConcaveEdge> edges = FindConcaveEdges(grid, N, walls);
            if (edges.Num() == 0) break;

            TMap<FSeparatorKey, FSeparatorWeight> weights = ComputeSeparatorWeights(edges, N);

            int32 alpha = TNumericLimits<int32>::Lowest();
            for (auto It = weights.CreateIterator(); It; ++It) alpha = (It.Value().Weight() > alpha) ? It.Value().Weight() : alpha;

            TArray<FSeparatorKey> M;
            for (auto It = weights.CreateIterator(); It; ++It)
                if (It.Value().Weight() == alpha) M.Add(It.Key());

            TArray<int32> chosenIndices;
            if (useExactMaxIS && M.Num() <= MaxExactSeparatorCount)
            {
                TArray<TArray<uint8_t>> adj;
                adj.SetNum(M.Num());
                for (int32 i = 0; i < M.Num(); ++i)
                {
                    adj[i].SetNum(M.Num());
                    for (int32 j = 0; j < M.Num(); ++j) adj[i][j] = 0;
                }
                for (int32 i = 0; i < M.Num(); ++i)
                    for (int32 j = i + 1; j < M.Num(); ++j)
                        if (M[i].Axis != M[j].Axis && Footprints.Intersect(M[i], M[j]))
                        {
                            adj[i][j] = 1;
                            adj[j][i] = 1;
                        }
                chosenIndices = MaxIndependentSet(M.Num(), adj);
            }
            else
            {
                chosenIndices = LargestAxisGroup(M);
            }

            if (chosenIndices.Num() == 0)
            {
                // Shouldn't happen (M is never empty here, and a single vertex is
                // always a valid independent set) - break defensively rather than loop forever.
                break;
            }

            for (int32 Idx : chosenIndices)
            {
                walls[M[Idx].Axis].Add(M[Idx].Coord);
            }
        }

        // ---- Carve blocks from the wall-induced grid arrangement ----
        TArray<int32> cuts[3];
        for (int32 a = 0; a < 3; ++a)
        {
            cuts[a].Add(0);
            TArray<int32> sorted;
            for (int32 w : walls[a]) sorted.Add(w);
            sorted.Sort();
            for (int32 w : sorted) cuts[a].Add(w);
            cuts[a].Add(N);
        }

        TArray<FVoxelBlock> blocks;
        for (int32 xi = 0; xi + 1 < cuts[0].Num(); ++xi)
            for (int32 yi = 0; yi + 1 < cuts[1].Num(); ++yi)
                for (int32 zi = 0; zi + 1 < cuts[2].Num(); ++zi)
                {
                    const int32 x0 = cuts[0][xi], x1 = cuts[0][xi + 1];
                    const int32 y0 = cuts[1][yi], y1 = cuts[1][yi + 1];
                    const int32 z0 = cuts[2][zi], z1 = cuts[2][zi + 1];

                    bool bFilled;
                    if (IsBoxHomogeneous(SAT, x0, y0, z0, x1, y1, z1, bFilled))
                    {
                        if (bFilled)
                        {
                            FVoxelBlock B;
                            B.X = x0;
                            B.Y = y0;
                            B.Z = z0;
                            B.SizeX = x1 - x0;
                            B.SizeY = y1 - y0;
                            B.SizeZ = z1 - z0;
                            blocks.Add(B);
                        }
                    }
                    else
                    {
                        // Should not happen once all concave edges are eliminated;
                        // fall back to per-voxel blocks for this one cell so the
                        // result stays correct even if this safety net ever fires.
                        for (int32 z = z0; z < z1; ++z)
                            for (int32 y = y0; y < y1; ++y)
                                for (int32 x = x0; x < x1; ++x)
                                    if (grid.IsFilled(x, y, z))
                                    {
                                        FVoxelBlock B;
                                        B.X = x;
                                        B.Y = y;
                                        B.Z = z;
                                        B.SizeX = 1;
                                        B.SizeY = 1;
                                        B.SizeZ = 1;
                                        blocks.Add(B);
                                    }
                    }
                }

        // ---- Merge pass: repeatedly fuse adjacent blocks that share a full face ----
        // Blocks are sorted so that every block sharing the SAME extents on the
        // other two axes becomes contiguous, ordered by position along the
        // merge axis; one linear scan then merges touching runs. This is
        // O(B log B) per axis per pass. The earlier all-pairs O(B^2) scan was
        // the actual bottleneck on large/noisy grids (many seconds once block
        // counts reach the thousands), not the concave-edge search itself.
        bool bMergedAny = true;
        int32 MergePasses = 0;
        while (bMergedAny && MergePasses < 6)
        {
            bMergedAny = false;
            ++MergePasses;
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                blocks.Sort(
                    [Axis](const FVoxelBlock& A, const FVoxelBlock& B)
                    {
                        int32 KeyA[5], KeyB[5];
                        if (Axis == 0)
                        {
                            KeyA[0] = A.Y;
                            KeyA[1] = A.SizeY;
                            KeyA[2] = A.Z;
                            KeyA[3] = A.SizeZ;
                            KeyA[4] = A.X;
                            KeyB[0] = B.Y;
                            KeyB[1] = B.SizeY;
                            KeyB[2] = B.Z;
                            KeyB[3] = B.SizeZ;
                            KeyB[4] = B.X;
                        }
                        else if (Axis == 1)
                        {
                            KeyA[0] = A.X;
                            KeyA[1] = A.SizeX;
                            KeyA[2] = A.Z;
                            KeyA[3] = A.SizeZ;
                            KeyA[4] = A.Y;
                            KeyB[0] = B.X;
                            KeyB[1] = B.SizeX;
                            KeyB[2] = B.Z;
                            KeyB[3] = B.SizeZ;
                            KeyB[4] = B.Y;
                        }
                        else
                        {
                            KeyA[0] = A.X;
                            KeyA[1] = A.SizeX;
                            KeyA[2] = A.Y;
                            KeyA[3] = A.SizeY;
                            KeyA[4] = A.Z;
                            KeyB[0] = B.X;
                            KeyB[1] = B.SizeX;
                            KeyB[2] = B.Y;
                            KeyB[3] = B.SizeY;
                            KeyB[4] = B.Z;
                        }
                        for (int32 k = 0; k < 5; ++k)
                        {
                            if (KeyA[k] != KeyB[k]) return KeyA[k] < KeyB[k];
                        }
                        return false;
                    });

                TArray<FVoxelBlock> Merged;
                int32 i = 0;
                while (i < blocks.Num())
                {
                    FVoxelBlock Cur = blocks[i];
                    int32 j = i + 1;
                    while (j < blocks.Num())
                    {
                        const FVoxelBlock& Next = blocks[j];
                        const bool bSameOther =
                            (Axis == 0 && Cur.Y == Next.Y && Cur.SizeY == Next.SizeY && Cur.Z == Next.Z &&
                             Cur.SizeZ == Next.SizeZ) ||
                            (Axis == 1 && Cur.X == Next.X && Cur.SizeX == Next.SizeX && Cur.Z == Next.Z &&
                             Cur.SizeZ == Next.SizeZ) ||
                            (Axis == 2 && Cur.X == Next.X && Cur.SizeX == Next.SizeX && Cur.Y == Next.Y && Cur.SizeY == Next.SizeY);
                        if (!bSameOther) break;  // sorted - no further matches for this run

                        const bool bTouches = (Axis == 0 && Cur.X + Cur.SizeX == Next.X) ||
                                              (Axis == 1 && Cur.Y + Cur.SizeY == Next.Y) ||
                                              (Axis == 2 && Cur.Z + Cur.SizeZ == Next.Z);
                        if (!bTouches) break;  // same group but a gap - stop extending, resume at j below

                        if (Axis == 0)
                            Cur.SizeX += Next.SizeX;
                        else if (Axis == 1)
                            Cur.SizeY += Next.SizeY;
                        else
                            Cur.SizeZ += Next.SizeZ;
                        ++j;
                        bMergedAny = true;
                    }
                    Merged.Add(Cur);
                    i = j;
                }
                blocks = Merged;
            }
        }

        return blocks;
    }

    TArray<FVoxelBlock> DecomposeMaxIS(const FVoxelGrid& Grid) { return DecomposeSeparatorBased(Grid, true); }
    TArray<FVoxelBlock> DecomposeIS1(const FVoxelGrid& Grid) { return DecomposeSeparatorBased(Grid, false); }

    // Cheap pre-flight complexity check: a single raw concave-edge count (no
    // walls placed yet), independent of the expensive iterative MaxIS/IS(1)
    // loop that follows it. Empirically, structured/rectilinear shapes score
    // well under 1.0; a voxelized sphere (curved surfaces rasterize into many
    // small staircase edges) scores around 1.2; random/noisy density scores
    // 3+ and climbs with grid size. There's no universal cutoff - tune this
    // to your own content - but something like edges/(N*N) > 2.0 is a
    // reasonable red flag for "MaxIS/IS(1) will likely be slow here, prefer
    // 3GDM/BSP or a lower-resolution collision proxy instead."
    float EstimateConcaveEdgeDensity(const FVoxelGrid& Grid)
    {
        const int32 N = (int32)Grid.GridSize;
        if (N <= 0) return 0.0f;
        TSet<int32> NoWalls[3];
        const TArray<FConcaveEdge> Edges = FindConcaveEdges(Grid, N, NoWalls);
        return (float)Edges.Num() / (float)(N * N);
    }

}  // namespace DecompVoxel