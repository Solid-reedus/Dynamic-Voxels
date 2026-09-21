#include "VoxelCollisionDecomposition.h"
#include "VoxelGrid.h"
#include "Containers/StaticArray.h"

namespace DecompVoxel
{
// ---------------- 3GDM (existing) ----------------
using FAxisOrder = TStaticArray<int32, 3>;

FAxisOrder MakeAxisOrder(int32 A, int32 B, int32 C)
{
    FAxisOrder Order;
    Order[0] = A;
    Order[1] = B;
    Order[2] = C;
    return Order;
}

TStaticArray<int32, 3> ToGridCoord(const FAxisOrder& Order, int32 I0, int32 I1, int32 I2)
{
    TStaticArray<int32, 3> Coord;
    Coord[Order[0]] = I0;
    Coord[Order[1]] = I1;
    Coord[Order[2]] = I2;
    return Coord;
}

struct FRun
{
    int32 Start = 0;
    int32 End = 0;
};

TArray<FRun> ExtractRuns(const FVoxelGrid& Grid, const FAxisOrder& Order, int32 N0, int32 I1, int32 I2)
{
    TArray<FRun> Runs;
    bool bInRun = false;
    int32 Start = 0;
    for (int32 I0 = 0; I0 <= N0; ++I0)
    {
        bool bOccupied = false;
        if (I0 < N0)
        {
            const TStaticArray<int32, 3> Coord = ToGridCoord(Order, I0, I1, I2);
            bOccupied = Grid.IsFilled(Coord[0], Coord[1], Coord[2]);
        }
        if (bOccupied && !bInRun)
        {
            Start = I0;
            bInRun = true;
        }
        else if (!bOccupied && bInRun)
        {
            Runs.Add(FRun{Start, I0});
            bInRun = false;
        }
    }
    return Runs;
}

struct FRunKey
{
    int32 Start = 0;
    int32 End = 0;
    bool operator==(const FRunKey& Other) const { return Start == Other.Start && End == Other.End; }
};
FORCEINLINE uint32 GetTypeHash(const FRunKey& Key) { return HashCombine(::GetTypeHash(Key.Start), ::GetTypeHash(Key.End)); }

struct FRect2D
{
    int32 S0 = 0, E0 = 0, S1 = 0, E1 = 0;
};
struct FRectKey
{
    int32 S0 = 0, E0 = 0, S1 = 0, E1 = 0;
    bool operator==(const FRectKey& Other) const
    {
        return S0 == Other.S0 && E0 == Other.E0 && S1 == Other.S1 && E1 == Other.E1;
    }
};
FORCEINLINE uint32 GetTypeHash(const FRectKey& Key)
{
    uint32 Hash = ::GetTypeHash(Key.S0);
    Hash = HashCombine(Hash, ::GetTypeHash(Key.E0));
    Hash = HashCombine(Hash, ::GetTypeHash(Key.S1));
    Hash = HashCombine(Hash, ::GetTypeHash(Key.E1));
    return Hash;
}

TArray<TArray<FRect2D>> BuildPlates(const FVoxelGrid& Grid, const FAxisOrder& Order, int32 N0, int32 N1, int32 N2)
{
    TArray<TArray<FRect2D>> Plates;
    Plates.SetNum(N2);
    for (int32 I2 = 0; I2 < N2; ++I2)
    {
        TMap<FRunKey, FRect2D> Active;
        for (int32 I1 = 0; I1 <= N1; ++I1)
        {
            TArray<FRun> Runs;
            if (I1 < N1) Runs = ExtractRuns(Grid, Order, N0, I1, I2);
            TSet<FRunKey> MatchedThisRow;
            for (const FRun& Run : Runs)
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
                    Plates[I2].Add(It->Value);
                    It.RemoveCurrent();
                }
            }
        }
    }
    return Plates;
}

struct FBlock3DInternal
{
    int32 S0 = 0, E0 = 0, S1 = 0, E1 = 0, S2 = 0, E2 = 0;
};

TArray<FBlock3DInternal> MergePlates(const TArray<TArray<FRect2D>>& Plates, int32 N2)
{
    TArray<FBlock3DInternal> Result;
    TMap<FRectKey, FBlock3DInternal> Active;
    for (int32 I2 = 0; I2 <= N2; ++I2)
    {
        TSet<FRectKey> MatchedThisSlice;
        if (I2 < N2)
        {
            for (const FRect2D& Rect : Plates[I2])
            {
                const FRectKey Key{Rect.S0, Rect.E0, Rect.S1, Rect.E1};
                if (FBlock3DInternal* Existing = Active.Find(Key))
                {
                    Existing->E2 = I2 + 1;
                }
                else
                {
                    Active.Add(Key, FBlock3DInternal{Rect.S0, Rect.E0, Rect.S1, Rect.E1, I2, I2 + 1});
                }
                MatchedThisSlice.Add(Key);
            }
        }
        for (auto It = Active.CreateIterator(); It; ++It)
        {
            if (!MatchedThisSlice.Contains(It->Key))
            {
                Result.Add(It->Value);
                It.RemoveCurrent();
            }
        }
    }
    return Result;
}

TArray<FVoxelBlock> DecomposeWithOrder(const FVoxelGrid& Grid, const FAxisOrder& Order)
{
    const int32 N = static_cast<int32>(Grid.GridSize);
    const TArray<TArray<FRect2D>> Plates = BuildPlates(Grid, Order, N, N, N);
    const TArray<FBlock3DInternal> Blocks4D = MergePlates(Plates, N);
    TArray<FVoxelBlock> Blocks;
    Blocks.Reserve(Blocks4D.Num());
    for (const FBlock3DInternal& B : Blocks4D)
    {
        const TStaticArray<int32, 3> MinCoord = ToGridCoord(Order, B.S0, B.S1, B.S2);
        TStaticArray<int32, 3> Size;
        Size[Order[0]] = B.E0 - B.S0;
        Size[Order[1]] = B.E1 - B.S1;
        Size[Order[2]] = B.E2 - B.S2;
        FVoxelBlock Block;
        Block.X = MinCoord[0];
        Block.Y = MinCoord[1];
        Block.Z = MinCoord[2];
        Block.SizeX = Size[0];
        Block.SizeY = Size[1];
        Block.SizeZ = Size[2];
        Blocks.Add(Block);
    }
    return Blocks;
}

TArray<FVoxelBlock> Decompose3GDM(const FVoxelGrid& Grid)
{
    static const FAxisOrder Orders[6] = {MakeAxisOrder(0, 1, 2),
                                         MakeAxisOrder(0, 2, 1),
                                         MakeAxisOrder(1, 0, 2),
                                         MakeAxisOrder(1, 2, 0),
                                         MakeAxisOrder(2, 0, 1),
                                         MakeAxisOrder(2, 1, 0)};
    TArray<FVoxelBlock> Best;
    int32 BestCount = TNumericLimits<int32>::Max();
    for (const FAxisOrder& Order : Orders)
    {
        TArray<FVoxelBlock> Blocks = DecomposeWithOrder(Grid, Order);
        if (Blocks.Num() < BestCount)
        {
            BestCount = Blocks.Num();
            Best = MoveTemp(Blocks);
        }
    }
    return Best;
}

// 3D summed-area table over a cube of side PaddedSize (>= actual grid size,
// padded with empty/zero beyond the real grid). Gives O(1) box-occupancy sums.
class F3DSummedAreaTable
{
public:
    void Build(const FVoxelGrid& Grid, int32 InPaddedSize)
    {
        PaddedSize = InPaddedSize;
        const int32 N = (int32)Grid.GridSize;
        const int32 S = PaddedSize + 1;
        Data.SetNum(S * S * S);
        for (int32 i = 0; i < Data.Num(); ++i) Data[i] = 0;

        for (int32 z = 0; z < PaddedSize; ++z)
            for (int32 y = 0; y < PaddedSize; ++y)
                for (int32 x = 0; x < PaddedSize; ++x)
                {
                    int32 v = (x < N && y < N && z < N && Grid.IsFilled(x, y, z)) ? 1 : 0;
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

static bool
IsBoxHomogeneous(const F3DSummedAreaTable& SAT, int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1, bool& OutFilled)
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

// ---------------- Octree ----------------
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

TArray<FVoxelBlock> DecomposeOctree(const FVoxelGrid& Grid)
{
    const int32 N = (int32)Grid.GridSize;
    int32 Size = 1;
    while (Size < N) Size <<= 1;  // next power of two >= N (QTD/OTD requirement)

    F3DSummedAreaTable SAT;
    SAT.Build(Grid, Size);

    TArray<FVoxelBlock> Out;
    OctreeRecurse(SAT, 0, 0, 0, Size, Out);
    return Out;
}

// ---------------- BSP ----------------
// Separator chosen, at each step, to minimize the number of occupied voxels
// immediately adjacent to the cut (i.e. cut where the object is "thinnest").
static void
BSPRecurse(const F3DSummedAreaTable& SAT, int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1, TArray<FVoxelBlock>& Out)
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

// Which of the two "other" axes (in increasing index order) an edge running
// along Axis is defined against.
static void OtherAxes(int32 Axis, int32& OutB, int32& OutC)
{
    if (Axis == 0)
    {
        OutB = 1;
        OutC = 2;
    }
    else if (Axis == 1)
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

static void MakeCoord(int32 Axis, int32 A, int32 B, int32 C, int32 OtherAxisB, int32 OtherAxisC, int32 OutXYZ[3])
{
    OutXYZ[Axis] = A;
    OutXYZ[OtherAxisB] = B;
    OutXYZ[OtherAxisC] = C;
}

struct FConcaveEdge
{
    int32 Axis = 0;            // axis the edge EXTENDS along
    int32 FixedB = 0;          // grid-line coord on OtherAxes(Axis).first
    int32 FixedC = 0;          // grid-line coord on OtherAxes(Axis).second
    int32 Start = 0, End = 0;  // [Start,End) along Axis
};

// Finds all maximal concave-edge runs, given the walls placed so far.
// A grid line (b,c) on the two axes perpendicular to Axis is "locally
// concave" at coordinate a if exactly 3 of the 4 surrounding voxels are
// filled (the classic reflex-corner pattern). Runs are additionally split
// wherever a wall already crosses the edge's own extending axis.
static TArray<FConcaveEdge> FindConcaveEdges(const FVoxelGrid& Grid, int32 N, const TSet<int32> Walls[3])
{
    TArray<FConcaveEdge> Edges;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        int32 AxB, AxC;
        OtherAxes(Axis, AxB, AxC);

        for (int32 b = 1; b < N; ++b)
        {
            if (Walls[AxB].Contains(b)) continue;  // already fully resolved
            for (int32 c = 1; c < N; ++c)
            {
                if (Walls[AxC].Contains(c)) continue;

                bool bInRun = false;
                int32 RunStart = 0;

                for (int32 a = 0; a <= N; ++a)
                {
                    bool bConcave = false;
                    if (a < N)
                    {
                        int32 Coord[3];
                        int32 Count = 0;
                        MakeCoord(Axis, a, b - 1, c - 1, AxB, AxC, Coord);
                        Count += Grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                        MakeCoord(Axis, a, b, c - 1, AxB, AxC, Coord);
                        Count += Grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                        MakeCoord(Axis, a, b - 1, c, AxB, AxC, Coord);
                        Count += Grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                        MakeCoord(Axis, a, b, c, AxB, AxC, Coord);
                        Count += Grid.IsFilled(Coord[0], Coord[1], Coord[2]) ? 1 : 0;
                        bConcave = (Count == 3);
                    }

                    const bool bWallHere = (a < N) && (a > 0) && Walls[Axis].Contains(a);

                    if (bInRun)
                    {
                        if (bConcave && !bWallHere)
                        {
                            continue;  // run extends through 'a'
                        }
                        // flush [RunStart, a)
                        FConcaveEdge E;
                        E.Axis = Axis;
                        E.FixedB = b;
                        E.FixedC = c;
                        E.Start = RunStart;
                        E.End = a;
                        Edges.Add(E);
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
    return Edges;
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

// Weight function w^(2) from the paper (Eq. 2): edges eliminated minus new
// concave edges the separator would create by cutting through others.
//
// The "intersected" term - for each candidate, how many same-axis edges it
// would cut through the middle of - is computed via a per-axis difference
// array in O(N + numEdges) rather than checking every (candidate, edge)
// pair, which is O(numEdges * numCandidates) and becomes the dominant cost
// (many seconds) on large, noisy grids with thousands of concave edges.
static TMap<FSeparatorKey, FSeparatorWeight> ComputeSeparatorWeights(const TArray<FConcaveEdge>& Edges, int32 N)
{
    TMap<FSeparatorKey, FSeparatorWeight> Weights;

    for (const FConcaveEdge& E : Edges)
    {
        int32 AxB, AxC;
        OtherAxes(E.Axis, AxB, AxC);
        const FSeparatorKey K1{AxB, E.FixedB};
        const FSeparatorKey K2{AxC, E.FixedC};
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
    for (const FConcaveEdge& E : Edges)
    {
        // Strictly-inside(Start,End) means coordinates Start+1 .. End-1.
        if (E.Start + 1 <= E.End - 1)
        {
            Diff[E.Axis][E.Start + 1] += 1;
            Diff[E.Axis][E.End] -= 1;
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

// Do two perpendicular separators actually touch the object? Uses a
// precomputed 2D "footprint" (projection) so this is O(1) per pair.
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

    bool AnyAt(int32 Axis1, int32 Coord1, int32 Axis2, int32 Coord2) const
    {
        // returns true if there's ANY filled voxel with coordinate Coord1 on
        // Axis1 and Coord2 on Axis2 (any value on the third axis).
        int32 a = Axis1, ca = Coord1, b = Axis2, cb = Coord2;
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
static void MISRecurse(const TArray<TArray<uint8_t>>& Adj,
                       TArray<int32>& Remaining,
                       TArray<int32>& Current,
                       TArray<int32>& Best)
{
    if (Current.Num() + Remaining.Num() <= Best.Num()) return;  // can't beat best
    if (Remaining.Num() == 0)
    {
        if (Current.Num() > Best.Num()) Best = Current;
        return;
    }

    const int32 V = Remaining[0];
    TArray<int32> RestWithoutV;
    for (int32 i = 1; i < Remaining.Num(); ++i) RestWithoutV.Add(Remaining[i]);

    // Branch: exclude V
    MISRecurse(Adj, RestWithoutV, Current, Best);

    // Branch: include V (drop anything adjacent to it)
    TArray<int32> Compatible;
    for (int32 U : RestWithoutV)
        if (!Adj[V][U]) Compatible.Add(U);
    Current.Add(V);
    MISRecurse(Adj, Compatible, Current, Best);
    Current.Pop();
}

static TArray<int32> MaxIndependentSet(int32 NumVertices, const TArray<TArray<uint8_t>>& Adj)
{
    TArray<int32> All;
    for (int32 i = 0; i < NumVertices; ++i) All.Add(i);
    TArray<int32> Current, Best;
    MISRecurse(Adj, All, Current, Best);
    return Best;
}

// Simple heuristic: largest single-axis group (always independent, since
// parallel separators never intersect). This is IS(1) from the paper.
static TArray<int32> LargestAxisGroup(const TArray<FSeparatorKey>& Candidates)
{
    TArray<int32> Groups[3];
    for (int32 i = 0; i < Candidates.Num(); ++i) Groups[Candidates[i].Axis].Add(i);
    int32 BestAxis = 0;
    for (int32 a = 1; a < 3; ++a)
        if (Groups[a].Num() > Groups[BestAxis].Num()) BestAxis = a;
    return Groups[BestAxis];
}

// Safety cutoff: above this many same-weight candidates, exact MaxIS search
// is skipped in favor of IS(1) to avoid combinatorial blow-up in-editor.
static constexpr int32 MaxExactSeparatorCount = 28;

static TArray<FVoxelBlock> DecomposeSeparatorBased(const FVoxelGrid& Grid, bool bUseExactMaxIS)
{
    const int32 N = (int32)Grid.GridSize;

    F3DSummedAreaTable SAT;
    SAT.Build(Grid, N);
    FFootprints Footprints;
    Footprints.Build(Grid, N);

    TSet<int32> Walls[3];

    for (int32 SafetyIterations = 0; SafetyIterations < N * 3 + 16; ++SafetyIterations)
    {
        TArray<FConcaveEdge> Edges = FindConcaveEdges(Grid, N, Walls);
        if (Edges.Num() == 0) break;

        TMap<FSeparatorKey, FSeparatorWeight> Weights = ComputeSeparatorWeights(Edges, N);

        int32 Alpha = TNumericLimits<int32>::Lowest();
        for (auto It = Weights.CreateIterator(); It; ++It) Alpha = (It.Value().Weight() > Alpha) ? It.Value().Weight() : Alpha;

        TArray<FSeparatorKey> M;
        for (auto It = Weights.CreateIterator(); It; ++It)
            if (It.Value().Weight() == Alpha) M.Add(It.Key());

        TArray<int32> ChosenIndices;
        if (bUseExactMaxIS && M.Num() <= MaxExactSeparatorCount)
        {
            TArray<TArray<uint8_t>> Adj;
            Adj.SetNum(M.Num());
            for (int32 i = 0; i < M.Num(); ++i)
            {
                Adj[i].SetNum(M.Num());
                for (int32 j = 0; j < M.Num(); ++j) Adj[i][j] = 0;
            }
            for (int32 i = 0; i < M.Num(); ++i)
                for (int32 j = i + 1; j < M.Num(); ++j)
                    if (M[i].Axis != M[j].Axis && Footprints.Intersect(M[i], M[j]))
                    {
                        Adj[i][j] = 1;
                        Adj[j][i] = 1;
                    }
            ChosenIndices = MaxIndependentSet(M.Num(), Adj);
        }
        else
        {
            ChosenIndices = LargestAxisGroup(M);
        }

        if (ChosenIndices.Num() == 0)
        {
            // Shouldn't happen (M is never empty here, and a single vertex is
            // always a valid independent set) - break defensively rather than loop forever.
            break;
        }

        for (int32 Idx : ChosenIndices)
        {
            Walls[M[Idx].Axis].Add(M[Idx].Coord);
        }
    }

    // ---- Carve blocks from the wall-induced grid arrangement ----
    TArray<int32> Cuts[3];
    for (int32 a = 0; a < 3; ++a)
    {
        Cuts[a].Add(0);
        TArray<int32> Sorted;
        for (int32 w : Walls[a]) Sorted.Add(w);
        Sorted.Sort();
        for (int32 w : Sorted) Cuts[a].Add(w);
        Cuts[a].Add(N);
    }

    TArray<FVoxelBlock> Blocks;
    for (int32 xi = 0; xi + 1 < Cuts[0].Num(); ++xi)
        for (int32 yi = 0; yi + 1 < Cuts[1].Num(); ++yi)
            for (int32 zi = 0; zi + 1 < Cuts[2].Num(); ++zi)
            {
                const int32 x0 = Cuts[0][xi], x1 = Cuts[0][xi + 1];
                const int32 y0 = Cuts[1][yi], y1 = Cuts[1][yi + 1];
                const int32 z0 = Cuts[2][zi], z1 = Cuts[2][zi + 1];

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
                        Blocks.Add(B);
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
                                if (Grid.IsFilled(x, y, z))
                                {
                                    FVoxelBlock B;
                                    B.X = x;
                                    B.Y = y;
                                    B.Z = z;
                                    B.SizeX = 1;
                                    B.SizeY = 1;
                                    B.SizeZ = 1;
                                    Blocks.Add(B);
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
            Blocks.Sort(
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
            while (i < Blocks.Num())
            {
                FVoxelBlock Cur = Blocks[i];
                int32 j = i + 1;
                while (j < Blocks.Num())
                {
                    const FVoxelBlock& Next = Blocks[j];
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
            Blocks = Merged;
        }
    }

    return Blocks;
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