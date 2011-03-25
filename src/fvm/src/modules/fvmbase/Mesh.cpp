#include <set>
#include "Mesh.h"
#include "StorageSite.h"
#include "CRConnectivity.h"
#include "Cell.h"
#include <cassert>
#include "KSearchTree.h"
#include "GeomFields.h"



#define epsilon 1e-6

int Mesh::_lastID = 0;

Mesh::Mesh(const int dimension):
  _dimension(dimension),
  _id(_lastID++),
  _cellZoneID(-1),
  _cells(0),
  _faces(0),
  _nodes(0),
  _ibFaces(0),
  _boundaryNodes(0),
  _interiorFaceGroup(),
  _faceGroups(),
  _boundaryGroups(),
  _interfaceGroups(),
  _connectivityMap(),
  _coordinates(),
  _boundaryNodeGlobalToLocalPtr(),
  _ibFaceList(),
  _numOfAssembleMesh(1),
  _isAssembleMesh(false),
  _isShell(false),
  _parentFaceGroupSite(0)
{
  _cells.setMesh(this);
  logCtor();
}

Mesh::Mesh( const int dimension,
            const Array<VecD3>&  faceNodesCoord ): 
  _dimension(dimension),
  _id(_lastID++),
  _cellZoneID(-1),
  _cells(0),
  _faces(0),
  _nodes(0),
  _ibFaces(0),
  _boundaryNodes(0),
  _interiorFaceGroup(),
  _faceGroups(),
  _boundaryGroups(),
  _interfaceGroups(),
  _connectivityMap(),
  _coordinates(),
  _boundaryNodeGlobalToLocalPtr(),
  _ibFaceList(),
  _numOfAssembleMesh(1),
  _isAssembleMesh(false),
  _isShell(false),
  _parentFaceGroupSite(0)
{
  int faceNodeCount = _dimension == 2 ? 2 :4;
  int totNodes      = faceNodesCoord.getLength();

  // counting duplicate nodes as well for 3d
  int totFaces      = totNodes / faceNodeCount;

  //check if this is corect integer division
  assert( (faceNodeCount*totFaces) == totNodes );
  //set sites
  StorageSite& faceSite = getFaces();
  StorageSite& nodeSite = getNodes();
  faceSite.setCount( totFaces );
  nodeSite.setCount( totNodes );
  //interior face group (we have only one interface for this
  createBoundaryFaceGroup(totFaces,0,0,"wall");
  
  //setting coordinates
  shared_ptr< Array<VecD3> > coord( new Array< VecD3 > ( totNodes ) );
  *coord = faceNodesCoord;
  setCoordinates( coord );
  
  //faceNodes constructor
  shared_ptr<CRConnectivity> faceNodes( new CRConnectivity(faceSite,
                                                           nodeSite) );

  faceNodes->initCount();
  //addCount
  for ( int i = 0; i < totFaces; i++ )
    faceNodes->addCount(i, faceNodeCount); 
  //finish count
  faceNodes->finishCount();
  //add operation 
  int face     = 0;
  int nodeIndx = 0;
  for( int i = 0; i < totFaces; i++ )
  {
      for( int j =0; j < faceNodeCount; j++ )
      {
          faceNodes->add(face, nodeIndx++);
      }
      face++;
  }
  //finish add
  faceNodes->finishAdd();
  
  //setting faceNodes
  SSPair key(&faceSite,&nodeSite);
  _connectivityMap[key] = faceNodes;

  logCtor();
}

Mesh::Mesh( const int dimension,
            const int nCells,
            const Array<VecD3>&  nodesCoord,
            const Array<int>& faceCellIndices,
            const Array<int>& faceNodeIndices,
            const Array<int>& faceNodeCount,
            const Array<int>& faceGroupSize
            ): 
  _dimension(dimension),
  _id(_lastID++),
  _cellZoneID(-1),
  _cells(0),
  _faces(0),
  _nodes(0),
  _ibFaces(0),
  _boundaryNodes(0),
  _interiorFaceGroup(),
  _faceGroups(),
  _boundaryGroups(),
  _interfaceGroups(),
  _connectivityMap(),
  _coordinates(),
  _boundaryNodeGlobalToLocalPtr(),
  _ibFaceList(),
  _numOfAssembleMesh(1),
  _isAssembleMesh(false),
  _isShell(false),
  _parentFaceGroupSite(0)
{
  int nFaces = faceNodeCount.getLength();
  int nNodes      = nodesCoord.getLength();

  StorageSite& faceSite = getFaces();
  StorageSite& nodeSite = getNodes();
  StorageSite& cellSite = getCells();
  faceSite.setCount( nFaces );
  nodeSite.setCount( nNodes );

  //interior face group (we have only one interface for this
  createInteriorFaceGroup(faceGroupSize[0]);
  
  int nFaceGroups = faceGroupSize.getLength();

  int offset = faceGroupSize[0];
  int nBoundaryFaces =0;
  for(int nfg=1; nfg<nFaceGroups; nfg++)
  {
      createBoundaryFaceGroup(faceGroupSize[nfg], offset, nfg, "wall");
      offset += faceGroupSize[nfg];
      nBoundaryFaces += faceGroupSize[nfg];
  }
  
  cellSite.setCount( nCells, nBoundaryFaces );
  
  //setting coordinates
  shared_ptr< Array<VecD3> > coord( new Array< VecD3 > ( nNodes ) );
  *coord = nodesCoord;
  setCoordinates( coord );
  
  
  //faceNodes constructor
  shared_ptr<CRConnectivity> faceNodes( new CRConnectivity(faceSite,
                                                           nodeSite) );

  faceNodes->initCount();

  shared_ptr<CRConnectivity> faceCells( new CRConnectivity(faceSite,
                                                           cellSite) );

  faceCells->initCount();

  
  //addCount
  for ( int f = 0; f < nFaces; f++ )
  {
      faceNodes->addCount(f, faceNodeCount[f]);
      faceCells->addCount(f, 2);
  }
  
  //finish count
  faceNodes->finishCount();
  faceCells->finishCount();
  
  //add operation 
  int nfn=0;
  int nfc=0;

  for( int f = 0; f < nFaces; f++ )
  {
      for( int j =0; j < faceNodeCount[f]; j++ )
      {
          faceNodes->add(f, faceNodeIndices[nfn++]);
      }
      faceCells->add(f,faceCellIndices[nfc++]);
      faceCells->add(f,faceCellIndices[nfc++]);
  }
  //finish add
  faceNodes->finishAdd();
  faceCells->finishAdd();
  
  //setting faceNodes
  SSPair key(&faceSite,&nodeSite);
  _connectivityMap[key] = faceNodes;
  
  SSPair key2(&faceSite,&cellSite);
  _connectivityMap[key2] = faceCells;



  logCtor();
}


Mesh::~Mesh()
{
  logDtor();
}




const StorageSite&
Mesh::createInteriorFaceGroup(const int size)
{
  _interiorFaceGroup = shared_ptr<FaceGroup>(new FaceGroup(size,0,_faces,0,"interior"));
  _faceGroups.push_back(_interiorFaceGroup);
  return _interiorFaceGroup->site;
}


const StorageSite&
Mesh::createInterfaceGroup(const int size, const int offset, const int id)
{
  shared_ptr<FaceGroup> fg(new FaceGroup(size,offset,_faces,id,"interface"));
  _faceGroups.push_back(fg);
  _interfaceGroups.push_back(fg);
  return fg->site;
}


const StorageSite&
Mesh::createBoundaryFaceGroup(const int size, const int offset, const int id, const string& boundaryType)
{
  shared_ptr<FaceGroup> fg(new FaceGroup(size,offset,_faces,id,boundaryType));
  _faceGroups.push_back(fg);
  _boundaryGroups.push_back(fg);
  return fg->site;
}


shared_ptr<Array<int> >
Mesh::createAndGetBNglobalToLocal() const
{
  if(!_boundaryNodeGlobalToLocalPtr)
  {
      const int nNodes = _nodes.getCount();
      _boundaryNodeGlobalToLocalPtr = shared_ptr<Array<int> >(new Array<int>(nNodes));
      Array<int>& globalToLocal = *_boundaryNodeGlobalToLocalPtr;
      globalToLocal = -1;
      int BoundaryNodeCount=0;
      int nLocal=0;
      foreach(const FaceGroupPtr fgPtr, getAllFaceGroups())
      {
	  const FaceGroup& fg = *fgPtr;
	  if (fg.groupType != "interior")
	  {
	      const StorageSite& BoundaryFaces = fg.site;
	      const CRConnectivity& BoundaryFaceNodes = getFaceNodes(BoundaryFaces);
	      const Array<int>& BFArray = BoundaryFaceNodes.getRow();
	      const Array<int>& BNArray = BoundaryFaceNodes.getCol();
	      const int nBFaces = BoundaryFaceNodes.getRowDim();
	      for(int i=0;i<nBFaces;i++)
	      {
		  for(int ip=BFArray[i];ip<BFArray[i+1];ip++)
		  {
		      const int j = BNArray[ip];
		      if (globalToLocal[j] == -1)
			globalToLocal[j] = nLocal++;
		  }
	      }
	  }
      }
      BoundaryNodeCount = nLocal;
  }
  return _boundaryNodeGlobalToLocalPtr;   
}


const StorageSite& Mesh::getBoundaryNodes() const 
{
  if(!_boundaryNodes)
  {
      const int nNodes = _nodes.getCount();
      shared_ptr<Array<int> > GlobalToLocalPtr = createAndGetBNglobalToLocal();
      Array<int>& GlobalToLocal = *GlobalToLocalPtr;
      int BoundaryNodeCount = 0;
      int nLocal = 0;
      for(int i=0;i<nNodes;i++)
      {
          if(GlobalToLocal[i] != -1)
	    nLocal++;
      }
      BoundaryNodeCount = nLocal;
      _boundaryNodes = new StorageSite(BoundaryNodeCount,0,0,0);
  }
  return *_boundaryNodes;
}


const ArrayBase& Mesh::getBNglobalToLocal() const
{
  return *(createAndGetBNglobalToLocal());
}


void Mesh::setConnectivity(const StorageSite& rowSite, const StorageSite& colSite,
	                       shared_ptr<CRConnectivity> conn)
{
  SSPair key(&rowSite,&colSite);
  _connectivityMap[key] = conn;
}

void Mesh::eraseConnectivity(const StorageSite& rowSite,
                             const StorageSite& colSite) const
{
  SSPair key(&rowSite,&colSite);
  _connectivityMap.erase(key);
}


const CRConnectivity&
Mesh::getAllFaceNodes() const
{
  SSPair key(&_faces,&_nodes);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;
  throw CException("face nodes not defined");
}

const CRConnectivity&
Mesh::getAllFaceCells() const
{
  SSPair key(&_faces,&_cells);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;
  throw CException("face cells not defined");
}

const CRConnectivity&
Mesh::getFaceCells(const StorageSite& faces) const
{
  SSPair key(&faces,&_cells);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;

  shared_ptr<CRConnectivity> thisFaceCells =
    getAllFaceCells().createOffset(faces,faces.getOffset(),faces.getCount());
  _connectivityMap[key] = thisFaceCells;
  return *thisFaceCells;
}

const CRConnectivity&
Mesh::getFaceNodes(const StorageSite& faces) const
{
  SSPair key(&faces,&_nodes);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;

  shared_ptr<CRConnectivity> thisFaceNodes =
    getAllFaceNodes().createOffset(faces,faces.getOffset(),faces.getCount());
  _connectivityMap[key] = thisFaceNodes;
  return *thisFaceNodes;
}

const CRConnectivity&
Mesh::getConnectivity(const StorageSite& from, const StorageSite& to) const
{
  SSPair key(&from,&to);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;
  throw CException("connectivity not defined");
}

const CRConnectivity&
Mesh::getCellNodes() const
{
  SSPair key(&_cells,&_nodes);
 


  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);

  if (pos != _connectivityMap.end())
    return *pos->second;


  const CRConnectivity& faceCells = getAllFaceCells();
  const CRConnectivity& faceNodes = getAllFaceNodes();
  
  SSPair keycf(&_cells,&_faces);
  shared_ptr<CRConnectivity> cellFaces = faceCells.getTranspose();
  shared_ptr<CRConnectivity> cellNodes = cellFaces->multiply(faceNodes,false);

  _connectivityMap[keycf] = cellFaces;
  _connectivityMap[key] = cellNodes;
  
  orderCellFacesAndNodes(*cellFaces, *cellNodes, faceNodes,
                         faceCells, *_coordinates);
  return *cellNodes;
}

const CRConnectivity&
Mesh::getCellFaces() const
{
  SSPair key(&_cells,&_faces);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;

  const CRConnectivity& faceCells = getAllFaceCells();
  shared_ptr<CRConnectivity> cellFaces = faceCells.getTranspose();

  _connectivityMap[key] = cellFaces;
  return *cellFaces;
}

CRConnectivity&
Mesh::getAllFaceCells()
{
  SSPair key(&_faces,&_cells);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;
  throw CException("face cells not defined");
}


const CRConnectivity&
Mesh::getCellCells() const
{
  SSPair key(&_cells,&_cells);
  ConnectivityMap::const_iterator pos = _connectivityMap.find(key);
  if (pos != _connectivityMap.end())
    return *pos->second;

  const CRConnectivity& faceCells = getAllFaceCells();
  const CRConnectivity& cellFaces = getCellFaces();
  shared_ptr<CRConnectivity> cellCells = cellFaces.multiply(faceCells,true);
  _connectivityMap[key] = cellCells;
  return *cellCells;
}

const CRConnectivity&
Mesh::getCellCells2() const
{
  if (!_cellCells2)
  { 
#ifdef FVM_PARALLEL
      //CRConnectivity constructor
      _cellCells2 = shared_ptr<CRConnectivity> ( new CRConnectivity(this->getCells(), this->getCells()) );
      //initCount
      _cellCells2->initCount();

      const int ncells = this->getCells().getCountLevel1();
      const int selfCount = this->getCells().getSelfCount();

      const CRConnectivity& cellCells = this->getCellCells();
      multimap<int,int>::const_iterator it0;
      multimap<int,int>::const_iterator it1;
       //loop over  cells
      for ( int n = 0; n < ncells; n++ ){ 
         set<int> setCells;
         for ( int k = 0; k < cellCells.getCount(n); k++ ){
             const int cellID1 = cellCells(n,k);
             setCells.insert( cellID1 );
             if ( cellID1 < selfCount ){ //it means inner cells use cellCells 
                for ( int i = 0; i < cellCells.getCount(cellID1); i++ ){
                    const int cellID2 = cellCells(cellID1,i);
                    setCells.insert(cellID2);
                }
             } else {
                for (  it1 = _cellCellsGlobal.equal_range(cellID1).first; it1 != _cellCellsGlobal.equal_range(cellID1).second; it1++ ){
                   const int globalCellID2 = it1->second;
                   const int localID2 = _globalToLocal.find(globalCellID2)->second;
                   if ( _globalToLocal.count(globalCellID2) > 0 )
                      setCells.insert(localID2);
                }
             }
         }
         //erase itself
         setCells.erase(n);
         _cellCells2->addCount(n,setCells.size());
      }
      //finish count
      _cellCells2->finishCount();
      //add cellcells2
      //loop over  cells
      for ( int n = 0; n < ncells; n++ ){ 
         set<int> setCells;
         for ( int k = 0; k < cellCells.getCount(n); k++ ){
             const int cellID1 = cellCells(n,k);
             setCells.insert( cellID1 );
             if ( cellID1 < selfCount ){ //it means inner cells use cellCells 
                for ( int i = 0; i < cellCells.getCount(cellID1); i++ ){
                    const int cellID2 = cellCells(cellID1,i);
                    setCells.insert(cellID2);
                }
             } else {
                for (  it1 = _cellCellsGlobal.equal_range(cellID1).first; it1 != _cellCellsGlobal.equal_range(cellID1).second; it1++ ){
                   const int globalCellID2 = it1->second;
                   const int localID2 = _globalToLocal.find(globalCellID2)->second;
                   if ( _globalToLocal.count(globalCellID2) > 0 )
                      setCells.insert(localID2);
                }
             }
         }
         //erase itself
         setCells.erase(n);
         foreach ( const set<int>::value_type cellID, setCells ){
             _cellCells2->add(n, cellID);
         }
      }
      //finish add
      _cellCells2->finishAdd();

     //unique numbering for cells to
    {
       const int ncount = _cellCells2->getRowSite().getCountLevel1();
       const Array<int>& row = _cellCells2->getRow();
       Array<int>& col = _cellCells2->getCol();
       for( int i = 0; i < ncount; i++ ){
          for ( int j = row[i]; j < row[i+1]; j++ ){
             const int globalID = (*_localToGlobal)[ col[j] ];
             col[j] = _globalToLocal[globalID];
          }
       }
     }
     //uniqing numberinf for face cells
     {
        CRConnectivity& faceCells = (const_cast<Mesh *>(this))->getAllFaceCells();
        const int nfaces = faceCells.getRowSite().getCount();
        const Array<int>& row = faceCells.getRow();
        Array<int>& col = faceCells.getCol();
        for( int i = 0; i < nfaces; i++ ){
           for ( int j = row[i]; j < row[i+1]; j++ ){
              const int globalID = (*_localToGlobal)[ col[j] ];
              col[j] = _globalToLocal[globalID];
           }
        }
        const StorageSite& cellSite = this->getCells();
        this->eraseConnectivity(cellSite,cellSite);
     }


      //get mappers to update cellCell2 localToGlobalMap and globalToLocalMapper
      map<int,int>& globalToLocalMapper = _cellCells2->getGlobalToLocalMapper();
      foreach( const  mapInt::value_type& mpos, _globalToLocal ){
         globalToLocalMapper[mpos.first] = mpos.second;
      }

      //localToGlobal need to be first created
      _cellCells2->resizeLocalToGlobalMap( this->getCells().getCountLevel1() );
      Array<int>& localToGlobal = *(_cellCells2->getLocalToGlobalMapPtr());
      for ( int i = 0; i < localToGlobal.getLength(); i++ ){
         localToGlobal[i] = (*_localToGlobal)[i];
      }

#endif

#ifndef FVM_PARALLEL
       const CRConnectivity& cellCells = getCellCells();
       _cellCells2 = cellCells.multiply(cellCells, true);
#endif



  }
  return *_cellCells2;
}

const CRConnectivity&
Mesh::getFaceCells2() const
{
  if (!_faceCells2)
  {
      const CRConnectivity& cellCells = getCellCells();
      const CRConnectivity& faceCells = getAllFaceCells();
      _faceCells2 = faceCells.multiply(cellCells, false);
  }
  return *_faceCells2;
}


void
Mesh::setFaceNodes(shared_ptr<CRConnectivity> faceNodes)
{
  SSPair key(&_faces,&_nodes);
  _connectivityMap[key] = faceNodes;
}


void
Mesh::setFaceCells(shared_ptr<CRConnectivity> faceCells)
{


  SSPair key(&_faces,&_cells);
  _connectivityMap[key] = faceCells;


}


void
Mesh::uniqueFaceCells()
{
     //uniqing numberinf for face cells
     {
        CRConnectivity& faceCells = (const_cast<Mesh *>(this))->getAllFaceCells();
        const int nfaces = faceCells.getRowSite().getCount();
        const Array<int>& row = faceCells.getRow();
        Array<int>& col = faceCells.getCol();
        for( int i = 0; i < nfaces; i++ ){
           for ( int j = row[i]; j < row[i+1]; j++ ){
              const int globalID = (*_localToGlobal)[ col[j] ];
              col[j] = _globalToLocal[globalID];
           }
        }
        const StorageSite& cellSite = this->getCells();
        this->eraseConnectivity(cellSite,cellSite);
     }
}

const Array<int>&
Mesh::getIBFaceList() const
{
  if (_ibFaceList)  return (*_ibFaceList);
  throw CException("ib face list not defined");
}


void 
Mesh::createGhostCellSiteScatter(  const PartIDMeshIDPair& id, shared_ptr<StorageSite> site )
{
  _ghostCellSiteScatterMap.insert( pair<PartIDMeshIDPair, shared_ptr<StorageSite> >( id, site ) );

}

void 
Mesh::createGhostCellSiteGather( const PartIDMeshIDPair& id, shared_ptr<StorageSite> site )
{
  _ghostCellSiteGatherMap.insert( pair<PartIDMeshIDPair, shared_ptr<StorageSite> >( id, site ) );

}

void 
Mesh::createGhostCellSiteScatterLevel1(  const PartIDMeshIDPair& id, shared_ptr<StorageSite> site )
{
  _ghostCellSiteScatterMapLevel1.insert( pair<PartIDMeshIDPair, shared_ptr<StorageSite> >( id, site ) );

}

void 
Mesh::createGhostCellSiteGatherLevel1( const PartIDMeshIDPair& id, shared_ptr<StorageSite> site )
{
  _ghostCellSiteGatherMapLevel1.insert( pair<PartIDMeshIDPair, shared_ptr<StorageSite> >( id, site ) );

}


//this 
void 
Mesh::createCellColor()
{
    //cellColor color ghost cells respect to self-inner cells
   _cellColor      = shared_ptr< Array<int> > ( new Array<int>( _cells.getCount() ) );
    //cellColorOther color ghost cells in respect to other partition,
    //if partition interface has aligned with mesh interface this will be different than _cellColor
   _cellColorOther = shared_ptr< Array<int> > ( new Array<int>( _cells.getCount() ) );

   *_cellColor = -1;
   *_cellColorOther = -1;
   _isAssembleMesh = true;
}

void
Mesh::createLocalGlobalArray()
{
   _localToGlobal      = shared_ptr< Array<int> > ( new Array<int>( _cells.getCountLevel1() ) );
   _localToGlobalNodes = shared_ptr< Array<int> > ( new Array<int>( _nodes.getCount() ) );
   *_localToGlobal  = -1;
}


void
Mesh::findCommonNodes(Mesh& other)
{
  StorageSite& nodes = _nodes;
  StorageSite& otherNodes = other._nodes;
  const int nNodes = nodes.getCount();
  const int nOtherNodes = otherNodes.getCount();

  const Array<VecD3>& coords = getNodeCoordinates();
  const Array<VecD3>& otherCoords = other.getNodeCoordinates();


  KSearchTree bNodeTree;

  // add all boundary nodes of this mesh to the tree
  {
      Array<bool> nodeMark(nNodes);
      nodeMark = false;
      foreach(const FaceGroupPtr fgPtr, getAllFaceGroups())
      {
          const FaceGroup& fg = *fgPtr;
          const StorageSite& faces = fg.site;
          if (fg.groupType!="interior")
          {
              const int nFaces = faces.getCount();
              const CRConnectivity& faceNodes = getFaceNodes(faces);
              for(int f=0; f<nFaces; f++)
              {
                  const int nFaceNodes = faceNodes.getCount(f);
                  for(int nn=0; nn<nFaceNodes; nn++)
                  {
                      const int n=faceNodes(f,nn);
                      if (!nodeMark[n])
                      {
                          nodeMark[n] = true;
                          bNodeTree.insert(coords[n],n);
                      }
                  }
              }
          }
      }
  }
  

  // loop over all the boundary nodes of the other mesh to find possible common ones
  Array<bool> nodeMark(nOtherNodes);

  typedef map<int,int> CommonNodesMap;
  CommonNodesMap commonNodesMap;

  Array<int> closest(2);
  
  nodeMark = false;

  
  foreach(const FaceGroupPtr fgPtr, other.getAllFaceGroups())
  {
      const FaceGroup& fg = *fgPtr;
      const StorageSite& faces = fg.site;
      if (fg.groupType!="interior")
      {
          const int nFaces = faces.getCount();
          const CRConnectivity& faceNodes = other.getFaceNodes(faces);

          for(int f=0; f<nFaces; f++)
          {
              const int nFaceNodes = faceNodes.getCount(f);
              for(int nn=0; nn<nFaceNodes; nn++)
              {
                  const int n=faceNodes(f,nn);
                  if (!nodeMark[n])
                  {
                      bNodeTree.findNeighbors(otherCoords[n],2,closest);
                      
                      double dist0 = mag(otherCoords[n] - coords[closest[0]]);
                      
                      // distance between the two closest point used as scale
                      
                      double distScale = mag(coords[closest[0]] - coords[closest[1]]);
                      
                      if (dist0 < distScale*epsilon)
                      {
                          // if another node has already been found as the
                          // closest for this one we have something wrong
                          if (commonNodesMap.find(closest[0]) != commonNodesMap.end())
                          {
                              throw CException("duplicate nodes on the mesh ?");
                          }
                          commonNodesMap.insert(make_pair(closest[0], n));
                      }
                      nodeMark[n] = true;
                  }
              }
          }
      }
  }

  const int nCommon = commonNodesMap.size();

  if (nCommon == 0)
    return;
  cout << "found " << nCommon << " common nodes " << endl;
  
  shared_ptr<IntArray> myCommonNodes(new IntArray(nCommon));
  shared_ptr<IntArray> otherCommonNodes(new IntArray(nCommon));

  int nc=0;
  foreach(CommonNodesMap::value_type& pos, commonNodesMap)
  {
      (*myCommonNodes)[nc] = pos.first;
      (*otherCommonNodes)[nc] = pos.second;
      nc++;
  }
  
  nodes.getCommonMap()[&otherNodes] = myCommonNodes;
  otherNodes.getCommonMap()[&nodes] = otherCommonNodes;
  
}

void
Mesh::findCommonFaces(StorageSite& faces, StorageSite& otherFaces,
                      const GeomFields& geomFields)
{
  const int count(faces.getCount());
  if (count != otherFaces.getCount())
    throw CException("face groups are not of the same length");

  const Array<VecD3>& coords =
    dynamic_cast<const Array<VecD3>& >(geomFields.coordinate[faces]);
  
  const Array<VecD3>& otherCoords =
    dynamic_cast<const Array<VecD3>& >(geomFields.coordinate[otherFaces]);

  const Array<VecD3>& area =
    dynamic_cast<const Array<VecD3>& >(geomFields.area[faces]);
  
  const Array<VecD3>& otherArea =
    dynamic_cast<const Array<VecD3>& >(geomFields.area[otherFaces]);

  KSearchTree thisFacesTree(coords);
  
  Array<int> closest(2);

  shared_ptr<IntArray> myCommonFaces(new IntArray(count));
  shared_ptr<IntArray> otherCommonFaces(new IntArray(count));
  
  for(int f=0; f<count; f++)
  {
      thisFacesTree.findNeighbors(otherCoords[f],2,closest);
      
      const int closestFace = closest[0];
      double dist0 = mag(otherCoords[f] - coords[closestFace]);
      
      // distance between the two closest point used as scale
      
      double distScale = mag(coords[closest[0]] - coords[closest[1]]);
      
      if (dist0 < distScale*epsilon)
      {
          double crossProductMag(mag2(cross(otherArea[f],area[closestFace])));
          if (crossProductMag > mag2(otherArea[f])*epsilon)
            throw CException("cross product is not small");
          
          (*otherCommonFaces)[f] = closestFace;
          (*myCommonFaces)[closestFace] = f;
          
      }
  }
  
  faces.getCommonMap()[&otherFaces] = myCommonFaces;
  otherFaces.getCommonMap()[&faces] = otherCommonFaces;
  
}


Mesh*
Mesh::extractBoundaryMesh()
{
  StorageSite& nodes = _nodes;
  const Array<VecD3>& coords = getNodeCoordinates();

  const int nodeCount = nodes.getCount();
  Array<int> globalToLocalNodes(nodeCount);

  globalToLocalNodes = -1;
  int bMeshNodeCount=0;
  int bMeshFaceCount=0;
  foreach(const FaceGroupPtr fgPtr, getAllFaceGroups())
  {
      const FaceGroup& fg = *fgPtr;
      const StorageSite& faces = fg.site;
      if (fg.groupType!="interior")
      {
          const int nFaces = faces.getCount();
          const CRConnectivity& faceNodes = getFaceNodes(faces);
          for(int f=0; f<nFaces; f++)
          {
              const int nFaceNodes = faceNodes.getCount(f);
              for(int nn=0; nn<nFaceNodes; nn++)
              {
                  const int n=faceNodes(f,nn);
                  if (globalToLocalNodes[n] == -1)
                  {
                      globalToLocalNodes[n] = bMeshNodeCount++;
                  }
              }
          }
          bMeshFaceCount += nFaces;
      }
  }


  Mesh *bMesh = new Mesh(_dimension);


  StorageSite& bMeshFaces = bMesh->getFaces();
  StorageSite& bMeshNodes = bMesh->getNodes();
  bMeshFaces.setCount( bMeshFaceCount );
  bMeshNodes.setCount( bMeshNodeCount );
  
  bMesh->createBoundaryFaceGroup(bMeshFaceCount,0,0,"wall");
  
  //setting coordinates
  shared_ptr< Array<VecD3> > bMeshCoordPtr( new Array< VecD3 > ( bMeshNodeCount ) );

  shared_ptr<IntArray> myCommonNodes(new IntArray(bMeshNodeCount));
  shared_ptr<IntArray> otherCommonNodes(new IntArray(bMeshNodeCount));

  for(int n=0; n<nodeCount; n++)
  {
      const int nLocal = globalToLocalNodes[n];
      if (nLocal >=0)
      {
          (*bMeshCoordPtr)[nLocal] = coords[n];
          (*myCommonNodes)[nLocal] = nLocal;
          (*otherCommonNodes)[nLocal] = n;
      }
  }
  nodes.getCommonMap()[&bMeshNodes] = myCommonNodes;
  bMeshNodes.getCommonMap()[&nodes] = otherCommonNodes;
         
  bMesh->setCoordinates( bMeshCoordPtr );
  
  //faceNodes constructor
  shared_ptr<CRConnectivity> bFaceNodes( new CRConnectivity(bMeshFaces,
                                                            bMeshNodes) );
  
  bFaceNodes->initCount();

  bMeshFaceCount=0;
  
  foreach(FaceGroupPtr fgPtr, getAllFaceGroups())
  {
      FaceGroup& fg = *fgPtr;
      StorageSite& faces = const_cast<StorageSite&>(fg.site);
      if (fg.groupType!="interior")
      {
          const int nFaces = faces.getCount();
          const CRConnectivity& faceNodes = getFaceNodes(faces);

          shared_ptr<IntArray> myCommonFaces(new IntArray(nFaces));
          shared_ptr<IntArray> otherCommonFaces(new IntArray(nFaces));

          for(int f=0; f<nFaces; f++)
          {
              const int nFaceNodes = faceNodes.getCount(f);
              bFaceNodes->addCount(bMeshFaceCount,nFaceNodes);
              (*myCommonFaces)[f] = bMeshFaceCount;
              (*otherCommonFaces)[f] = f;
              bMeshFaceCount++;
          }

          faces.getCommonMap()[&bMeshFaces] = myCommonFaces;
          bMeshFaces.getCommonMap()[&faces] = otherCommonFaces;
      }
  }

  bFaceNodes->finishCount();
  bMeshFaceCount=0;

  foreach(const FaceGroupPtr fgPtr, getAllFaceGroups())
  {
      const FaceGroup& fg = *fgPtr;
      const StorageSite& faces = fg.site;
      if (fg.groupType!="interior")
      {
          const int nFaces = faces.getCount();
          const CRConnectivity& faceNodes = getFaceNodes(faces);
          for(int f=0; f<nFaces; f++)
          {
              const int nFaceNodes = faceNodes.getCount(f);
              for(int nn=0; nn<nFaceNodes; nn++)
              {
                  const int n=faceNodes(f,nn);
                  const int nLocal = globalToLocalNodes[n];
                  bFaceNodes->add(bMeshFaceCount,nLocal);
              }
              bMeshFaceCount++;
          }
      }
  }
  
  bFaceNodes->finishAdd();
  //setting faceNodes
  SSPair key(&bMeshFaces,&bMeshNodes);
  bMesh->_connectivityMap[key] = bFaceNodes;

  return bMesh;

}

Mesh*
Mesh::extrude(int nz, double zmax, bool boundaryOnly)
{
  if (boundaryOnly)
    nz = 1;
  
  if (_dimension != 2)
    throw CException("can only extrude two dimensional mesh");

  const int myNCells = _cells.getSelfCount();
  const int myNFaces = _faces.getSelfCount();
  const int myNInteriorFaces = _interiorFaceGroup->site.getCount();
  const int myNBoundaryFaces = myNFaces - myNInteriorFaces;
  const int myNNodes = _nodes.getSelfCount();
  const CRConnectivity& myCellNodes = getCellNodes();
 
  const int eNInteriorFaces_rib = boundaryOnly ? 0 : nz*myNInteriorFaces;
  const int eNInteriorFaces_cap = (nz-1)*myNCells;
  
  const int eNInteriorFaces = eNInteriorFaces_rib + eNInteriorFaces_cap;
  const int eNBoundaryFaces = nz*myNBoundaryFaces + 2*myNCells;
  const int eNFaces = eNInteriorFaces + eNBoundaryFaces;
  const int eNInteriorCells = boundaryOnly ? 0 : nz*myNCells;
  const int eNBoundaryCells = boundaryOnly ? 0 : eNBoundaryFaces;
  
  const int eNNodes = (nz+1)*myNNodes;
  
  Mesh *eMesh = new Mesh(3);

  // set nodes
  StorageSite& eMeshNodes = eMesh->getNodes();
  eMeshNodes.setCount( eNNodes );

  const Array<VecD3>& myCoords = getNodeCoordinates();
  shared_ptr< Array<VecD3> > eMeshCoordPtr( new Array< VecD3 > ( eNNodes ) );
  Array<VecD3>& eCoords = *eMeshCoordPtr;
  const double dz = zmax/nz;

  const double z0 = -zmax/2.0;
  for(int k=0, en=0; k<=nz; k++)
  {
      const double z = z0 + dz*k;
      for(int n=0; n<myNNodes; n++)
      {
          eCoords[en][0] = myCoords[n][0];
          eCoords[en][1] = myCoords[n][1];
          eCoords[en][2] = z;
          en++;
      }
  }

  eMesh->setCoordinates( eMeshCoordPtr );
  
          
  // set cells
  
  StorageSite& eMeshCells = eMesh->getCells();
  eMeshCells.setCount( eNInteriorCells, eNBoundaryCells);
  

  // set faces
  
  StorageSite& eMeshFaces = eMesh->getFaces();
  eMeshFaces.setCount(eNFaces);


  shared_ptr<CRConnectivity> eFaceNodes( new CRConnectivity(eMeshFaces,
                                                            eMeshNodes) );

  shared_ptr<CRConnectivity> eFaceCells( new CRConnectivity(eMeshFaces,
                                                            eMeshCells) );

  // set counts for face cells and face nodes

  
  eFaceNodes->initCount();
  eFaceCells->initCount();

  int f = 0;

  if (!boundaryOnly)
  {
      // rib faces first
      for(; f<eNInteriorFaces_rib; f++)
      {
          eFaceNodes->addCount(f,4);
          eFaceCells->addCount(f,2);
      }
      
      // interior cap faces
      for(int k=1; k<nz; k++)
      {
          for(int c=0; c<myNCells; c++)
          {
              eFaceNodes->addCount(f, myCellNodes.getCount(c));
              eFaceCells->addCount(f, 2);
              f++;
          }
      }
      
      eMesh->createInteriorFaceGroup(eNInteriorFaces);
  }
  

  // z = 0 faces
  eMesh->createBoundaryFaceGroup(myNCells,  f, 10000, "wall");
  for(int c=0; c<myNCells; c++)
  {
      eFaceNodes->addCount(f, myCellNodes.getCount(c));
      eFaceCells->addCount(f, 2);
      f++;
  }
  
  // z = zmax faces
  eMesh->createBoundaryFaceGroup(myNCells,  f, 10001, "wall");
  for(int c=0; c<myNCells; c++)
  {
      eFaceNodes->addCount(f, myCellNodes.getCount(c));
      eFaceCells->addCount(f, 2);
      f++;
  }
  
  foreach(const FaceGroupPtr fgPtr, getAllFaceGroups())
  {
      const FaceGroup& fg = *fgPtr;
      const StorageSite& faces = fg.site;
      if (fg.groupType!="interior")
      {
          const int nBFaces = faces.getCount();

          eMesh->createBoundaryFaceGroup(nBFaces*nz,  f, fg.id, fg.groupType);

          for(int k=0;  k<nz; k++)
            for(int bf=0; bf<nBFaces; bf++)
            {
                eFaceNodes->addCount(f,4);
                eFaceCells->addCount(f,2);
                f++;
            }
      }
  }
  
  eFaceNodes->finishCount();
  eFaceCells->finishCount();


  // now set the indices of face cells and nodes

  
  f = 0;

  
  // rib faces first

  const CRConnectivity& myFaceNodes = getAllFaceNodes();
  const CRConnectivity& myFaceCells = getAllFaceCells();

  if (!boundaryOnly)
  {
      for(int k=0; k<nz; k++)
      {
          const int eCellOffset = k*myNCells;
          const int eNodeOffset = k*myNNodes;
          
          for(int myf=0; myf<myNInteriorFaces; myf++)
          {
              const int myNode0 = myFaceNodes(myf,0);
              const int myNode1 = myFaceNodes(myf,1);
              
              const int myCell0 = myFaceCells(myf,0);
              const int myCell1 = myFaceCells(myf,1);
              
              
              eFaceNodes->add(f, myNode0 + eNodeOffset);
              eFaceNodes->add(f, myNode1 + eNodeOffset );
              eFaceNodes->add(f, myNode1 + eNodeOffset + myNNodes);
              eFaceNodes->add(f, myNode0 + eNodeOffset + myNNodes);
              
              eFaceCells->add(f, myCell0 + eCellOffset);
              eFaceCells->add(f, myCell1 + eCellOffset);
              
              f++;
          }
      }
      
      // interior cap faces
      for(int k=1; k<nz; k++)
      {
          const int eCellOffset = k*myNCells;
          const int eNodeOffset = k*myNNodes;
          
          for(int c=0; c<myNCells; c++)
          {
              const int nCellNodes = myCellNodes.getCount(c);
              for(int nnc=0; nnc<nCellNodes; nnc++)
              {
                  eFaceNodes->add(f, myCellNodes(c,nnc) + eNodeOffset);
              }
              
              eFaceCells->add(f, c + eCellOffset - myNCells);
              eFaceCells->add(f, c + eCellOffset);
              f++;
          }
      }
  }

  // counter for boundary faces 
  int ebf = 0;


  // z = 0 faces
  {  
      const int eCellOffset = 0;
      const int eNodeOffset = 0;

      for(int c=0; c<myNCells; c++)
      {
          const int nCellNodes = myCellNodes.getCount(c);
          
          // reverse order of face nodes
          for(int nnc=nCellNodes-1; nnc>=0; nnc--)
          {
              eFaceNodes->add(f, myCellNodes(c,nnc) + eNodeOffset);
          }
          
          eFaceCells->add(f, c + eCellOffset);
          eFaceCells->add(f, ebf + eNInteriorCells);
          f++;
          ebf++;
      }
      
  }
  
  // z = zmax faces
  {  
      const int eCellOffset = (nz-1)*myNCells;
      const int eNodeOffset = nz*myNNodes;

      for(int c=0; c<myNCells; c++)
      {
          const int nCellNodes = myCellNodes.getCount(c);
          
          // reverse order of face nodes
          for(int nnc=0; nnc<nCellNodes; nnc++)
          {
              eFaceNodes->add(f, myCellNodes(c,nnc) + eNodeOffset);
          }
          
          eFaceCells->add(f, c + eCellOffset);
          eFaceCells->add(f, ebf + eNInteriorCells);
          f++;
          ebf++;
      }
  }
  

  // rib boundary faces
  foreach(const FaceGroupPtr fgPtr, getAllFaceGroups())
  {
      const FaceGroup& fg = *fgPtr;
      const StorageSite& faces = fg.site;
      if (fg.groupType!="interior")
      {
          const int nBFaces = faces.getCount();

          const CRConnectivity& bFaceNodes = getFaceNodes(faces);
          const CRConnectivity& bFaceCells = getFaceCells(faces);
          
          for(int k=0;  k<nz; k++)
          {
              const int eCellOffset = k*myNCells;
              const int eNodeOffset = k*myNNodes;
              for(int bf=0; bf<nBFaces; bf++)
              {
                  const int myNode0 = bFaceNodes(bf,0);
                  const int myNode1 = bFaceNodes(bf,1);
                  
                  const int myCell0 = bFaceCells(bf,0);
          
                  
                  eFaceNodes->add(f, myNode0 + eNodeOffset);
                  eFaceNodes->add(f, myNode1 + eNodeOffset);
                  eFaceNodes->add(f, myNode1 + eNodeOffset + myNNodes);
                  eFaceNodes->add(f, myNode0 + eNodeOffset + myNNodes);
                  
                  eFaceCells->add(f, myCell0 + eCellOffset);
                  eFaceCells->add(f, ebf + eNInteriorCells);
                  
                  f++;
                  ebf++;
              }
          }
      }
      
  }

  eFaceNodes->finishAdd();
  eFaceCells->finishAdd();

 //setting faceNodes
  SSPair key1(&eMeshFaces,&eMeshNodes);
  eMesh->_connectivityMap[key1] = eFaceNodes;

  if (!boundaryOnly)
  {
      SSPair key2(&eMeshFaces,&eMeshCells);
      eMesh->_connectivityMap[key2] = eFaceCells;
  }
  
  return eMesh;
}

const FaceGroup&
Mesh::getFaceGroup(const int fgId) const
{
  foreach(const FaceGroupPtr fgPtr, getAllFaceGroups())
  {
      const FaceGroup& fg = *fgPtr;
      if (fg.id == fgId)
        return fg;
  }
  throw CException("no face group with given id");
}

Mesh*
Mesh::createShell(const int fgId, Mesh& otherMesh, const int otherFgId)
{
  typedef Array<int> IntArray;
  
  const FaceGroup& fg = getFaceGroup(fgId);
  const StorageSite& fgSite = fg.site;
  StorageSite& cells = getCells();


  StorageSite& otherCells = otherMesh.getCells();
  
  const CRConnectivity& faceCells = getFaceCells(fgSite);

  const int count = fgSite.getSelfCount();

  Mesh* shellMesh = new Mesh(_dimension);

  shellMesh->_isShell = true;
  shellMesh->_parentFaceGroupSite = &fgSite;
  
  StorageSite& sMeshCells = shellMesh->getCells();

  // as many cells as there are faces, plus twice the number of ghost cells
  sMeshCells.setCount( count, 2*count);


  // we will number the cells in the shell mesh in the same order as
  // the faces on the left mesh
  
  // create a mapping from the cells in the left mesh to the smesh
  // cells using the faceCell connectivity
  map<int, int> leftCellsToSCells;
  for(int f=0; f<count; f++)
  {
      const int c0 = faceCells(f,0);
      const int c1 = faceCells(f,1);
      leftCellsToSCells[c0] = f;
      leftCellsToSCells[c1] = f;
  }

  // since there can be more than one face group in common between the
  // two meshes, we need to split up the existing gather scatter index
  // arrays into two sets, one for the cells that are connected
  // through the faces in the current face group and one for the rest.

  
  StorageSite::ScatterMap& lScatterMap = cells.getScatterMap();
  StorageSite::ScatterMap& rScatterMap = otherCells.getScatterMap();

  shared_ptr<IntArray> L2RScatterOrigPtr = lScatterMap[&otherCells];
  shared_ptr<IntArray> R2LScatterOrigPtr = rScatterMap[&cells];
  
  
  StorageSite::GatherMap& lGatherMap = cells.getGatherMap();
  StorageSite::GatherMap& rGatherMap = otherCells.getGatherMap();
  
  shared_ptr<IntArray> L2RGatherOrigPtr = lGatherMap[&otherCells];
  shared_ptr<IntArray> R2LGatherOrigPtr = rGatherMap[&cells];

  // the new1 arrays will have the size count and the new2 ones will
  // be the current size of the arrays - count

  const int countOrig = L2RScatterOrigPtr->getLength();
  const int count2 = countOrig - count;

  shared_ptr<IntArray> L2RScatterNew1Ptr( new IntArray(count) );
  shared_ptr<IntArray> R2LScatterNew1Ptr( new IntArray(count) );
  shared_ptr<IntArray> L2RGatherNew1Ptr( new IntArray(count) );
  shared_ptr<IntArray> R2LGatherNew1Ptr( new IntArray(count) );

  shared_ptr<IntArray> L2RScatterNew2Ptr;
  shared_ptr<IntArray> R2LScatterNew2Ptr;
  shared_ptr<IntArray> L2RGatherNew2Ptr;
  shared_ptr<IntArray> R2LGatherNew2Ptr;

  if (count2 > 0)
  {
      L2RScatterNew2Ptr = shared_ptr<IntArray>( new IntArray(count2) );
      R2LScatterNew2Ptr = shared_ptr<IntArray>( new IntArray(count2) );
      L2RGatherNew2Ptr = shared_ptr<IntArray>( new IntArray(count2) );
      R2LGatherNew2Ptr = shared_ptr<IntArray>( new IntArray(count2) );
  }

  for(int i=0, nc1=0, nc2=0; i<countOrig; i++)
  {
      const int lcg = (*L2RGatherOrigPtr)[i];
      // if the left cell is in the current face groups neighbours it goes into the new1 
      if (leftCellsToSCells.find(lcg) != leftCellsToSCells.end())
      {
          (*L2RScatterNew1Ptr)[nc1] = (*L2RScatterOrigPtr)[i];
          (*R2LScatterNew1Ptr)[nc1] = (*R2LScatterOrigPtr)[i];
          (*L2RGatherNew1Ptr)[nc1] = (*L2RGatherOrigPtr)[i];
          (*R2LGatherNew1Ptr)[nc1] = (*R2LGatherOrigPtr)[i];
          nc1++;
      }
      else
      {
          (*L2RScatterNew2Ptr)[nc2] = (*L2RScatterOrigPtr)[i];
          (*R2LScatterNew2Ptr)[nc2] = (*R2LScatterOrigPtr)[i];
          (*L2RGatherNew2Ptr)[nc2] = (*L2RGatherOrigPtr)[i];
          (*R2LGatherNew2Ptr)[nc2] = (*R2LGatherOrigPtr)[i];
          nc2++;
      }
  }        
  
  // create the gather and
  // scatter arrays for the smesh cells accordingly
  
  shared_ptr<IntArray> s2LScatterPtr( new IntArray(count) );
  shared_ptr<IntArray> s2RScatterPtr( new IntArray(count) );

  shared_ptr<IntArray> L2sGatherPtr( new IntArray(count) );
  shared_ptr<IntArray> R2sGatherPtr( new IntArray(count) );
  
  for(int i=0; i<count; i++)
  {
      // the left cell being gathered to
      const int lcg = (*L2RGatherNew1Ptr)[i];

      // the corresponding cell in the shell mesh should be scattering to both L and R
      (*s2LScatterPtr)[i] = leftCellsToSCells[lcg];
      (*s2RScatterPtr)[i] = leftCellsToSCells[lcg];

      // the left cell being scattered from
      const int lcs = (*L2RScatterNew1Ptr)[i];

      // this should be gathered in the left ghost cell of shell mesh
      (*L2sGatherPtr)[i] = leftCellsToSCells[lcs] + count;

      // the right cell is gathered in the right ghost
      (*R2sGatherPtr)[i] = leftCellsToSCells[lcs] + 2*count;
     
  }

  // set the gather scatter arrays in the smesh cells
  sMeshCells.getScatterMap()[&cells] = s2LScatterPtr;
  sMeshCells.getScatterMap()[&otherCells] = s2LScatterPtr;
  sMeshCells.getGatherMap()[&cells] = L2sGatherPtr;
  sMeshCells.getGatherMap()[&otherCells] = R2sGatherPtr;

  // replace the existing arrays in left and right maps and add the
  // new ones

  lScatterMap.erase(&otherCells);
  if (count2 >0)
    lScatterMap[&otherCells] = L2RScatterNew2Ptr;
  lScatterMap[&sMeshCells] = L2RScatterNew1Ptr;
  
  lGatherMap.erase(&otherCells);
  if (count2 >0)
    lGatherMap[&otherCells] = L2RGatherNew2Ptr;
  lGatherMap[&sMeshCells] = L2RGatherNew1Ptr;
  
  rScatterMap.erase(&cells);
  if (count2 >0)
    rScatterMap[&cells] = R2LScatterNew2Ptr;
  rScatterMap[&sMeshCells] = R2LScatterNew1Ptr;
  
  rGatherMap.erase(&cells);
  if (count2 >0)
    rGatherMap[&cells] = R2LGatherNew2Ptr;
  rGatherMap[&sMeshCells] = R2LGatherNew1Ptr;

  // create the cell cell connectivity for the shell mesh
  shared_ptr<CRConnectivity> sCellCells(new CRConnectivity(sMeshCells,sMeshCells));
  sCellCells->initCount();

  // two neighbours for the first count cells, one for the rest
  for(int i=0; i<count; i++)
  {
      sCellCells->addCount(i,2);
      sCellCells->addCount(i+count,1);
      sCellCells->addCount(i+2*count,1);
  }
  
  sCellCells->finishCount();

  for(int i=0; i<count; i++)
  {
      sCellCells->add(i,i+count);
      sCellCells->add(i,i+2*count);

      sCellCells->add(i+count,i);
      sCellCells->add(i+2*count,i);
  }

  sCellCells->finishAdd();

  SSPair key(&sMeshCells,&sMeshCells);

  shellMesh->_connectivityMap[key] = sCellCells;

  return shellMesh;
}



 
void 
Mesh::createCellCellsGhostExt()
{
#ifdef FVM_PARALLEL
   createRowColSiteCRConn();
   countCRConn();
   addCRConn();
   //CRConnectivityPrint(this->getCellCellsGhostExt(), 0, "cellCells");
#endif   
}
  
//fill countArray (both mesh and partition) and only gatherArray for mesh
void
Mesh::createScatterGatherCountsBuffer()
{
#ifdef FVM_PARALLEL
    //SENDING allocation and filling
    const StorageSite& site     = this->getCells();
    const CRConnectivity& cellCells = this->getCellCells();
    const StorageSite::ScatterMap& scatterMap = site.getScatterMap();
    foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
        const StorageSite&  oSite = *mpos.first;
        //checking if storage site is only site or ghost site, we only communicate ghost site ( oSite.getCount() == -1 ) 
        const Array<int>& scatterArray = *mpos.second;
        //key site
        EntryIndex e(&site,&oSite);
        //allocate array
        _sendCounts[e] = shared_ptr< Array<int>   > ( new Array<int> (scatterArray.getLength()) ); 
        //fill send array
        Array<int>& sendArray = dynamic_cast< Array<int>& > ( *_sendCounts[e] );
        for( int i = 0; i < scatterArray.getLength(); i++ ){
            const int cellID = scatterArray[i];
            sendArray[i] = cellCells.getCount(cellID);
        }
    }
                     	     

    //RECIEVING allocation (filling will be done by MPI Communication)
    const StorageSite::GatherMap& gatherMap = site.getGatherMap();
    foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
    const StorageSite&  oSite = *mpos.first;
    const Array<int>& gatherArray = *mpos.second;
    //checking if storage site is only site or ghost site, we only communicate ghost site ( oSite.getCount() == -1 ) 
    EntryIndex e(&oSite,&site);
    //allocate array
       _recvCounts[e] = shared_ptr< Array<int> > ( new Array<int> (gatherArray.getLength()) ); 
    }
#endif       
 }

//fill countArray (both mesh and partition) and only gatherArray for mesh
void
Mesh::recvScatterGatherCountsBufferLocal()
{
#ifdef FVM_PARALLEL
   const StorageSite& site     = this->getCells();
   const StorageSite::GatherMap& gatherMap = site.getGatherMap();
   foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
      const StorageSite&  oSite = *mpos.first;
      //checking if storage site is only site or ghost site, we only communicate ghost site ( oSite.getCount() == -1 ) 
      EntryIndex e(&oSite,&site);
      //allocate array
      //mesh interface can be done know
      if ( oSite.getGatherProcID() == - 1) {
	   const Mesh& otherMesh = oSite.getMesh();
          *_recvCounts[e] = otherMesh.getSendCounts(e);
      } 
   }
#endif   
}



void
Mesh::syncCounts()
{
#ifdef FVM_PARALLEL
    //SENDING
    const int  request_size = get_request_size();
    MPI::Request   request_send[ request_size ];
    MPI::Request   request_recv[ request_size ];
    int indxSend = 0;
    int indxRecv = 0;
    const StorageSite&    site      = this->getCells();
    const StorageSite::ScatterMap& scatterMap = site.getScatterMap();
    foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
        const StorageSite&  oSite = *mpos.first;
        EntryIndex e(&site,&oSite);
        //checking if storage site is only site or ghost site, we only communicate ghost site ( oSite.getCount() == -1 ) 
        ArrayBase& sendArray = *_sendCounts[e];

        //loop over surround indices and itself
        int to_where  = oSite.getGatherProcID();
        if ( to_where != -1 ){
           int mpi_tag = oSite.getTag();
           request_send[indxSend++] =  
                 MPI::COMM_WORLD.Isend( sendArray.getData(), sendArray.getDataSize(), MPI::BYTE, to_where, mpi_tag );
        }
     }
     //RECIEVING
     //getting values from other meshes to fill g
     const StorageSite::GatherMap& gatherMap = site.getGatherMap();
     foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
        const StorageSite&  oSite = *mpos.first;
        //checking if storage site is only site or ghost site, we only communicate ghost site ( oSite.getCount() == -1 ) 
        EntryIndex e(&oSite,&site);
        ArrayBase& recvArray = *_recvCounts[e];
        int from_where       = oSite.getGatherProcID();
        if ( from_where != -1 ){
           int mpi_tag = oSite.getTag();
           request_recv[indxRecv++] =  
                  MPI::COMM_WORLD.Irecv( recvArray.getData(), recvArray.getDataSize(), MPI::BYTE, from_where, mpi_tag );
        }
     }

     int count  = get_request_size();
     MPI::Request::Waitall( count, request_recv );
     MPI::Request::Waitall( count, request_send );

#endif

}


//fill scatterArray (both mesh and partition) and only gatherArray for mesh
void    
Mesh::createScatterGatherIndicesBuffer()
{
#ifdef FVM_PARALLEL
    //SENDING allocation and filling
    const StorageSite& site     = this->getCells();
    const CRConnectivity& cellCells = this->getCellCells();
    const Array<int>&   localToGlobal = this->getLocalToGlobal();
    const StorageSite::ScatterMap& scatterMap = site.getScatterMap();
    foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
        const StorageSite&  oSite = *mpos.first;
        const Array<int>& scatterArray = *mpos.second;
        //loop over surround indices and itself for sizing
        EntryIndex e(&site,&oSite);
        //allocate array
        int sendSize = 0;
        for ( int i = 0; i < scatterArray.getLength(); i++ ){
           sendSize += cellCells.getCount( scatterArray[i] );
        }
        _sendIndices[e] = shared_ptr< Array<int>   > ( new Array<int> (sendSize) ); 
        //fill send array
        Array<int>& sendArray = dynamic_cast< Array<int>&   > ( *_sendIndices[e] );
        int indx = 0;
        for( int i = 0; i < scatterArray.getLength(); i++ ){
           const int cellID = scatterArray[i];
           for ( int j = 0; j < cellCells.getCount(cellID); j++ ){
              sendArray[indx] = localToGlobal[ cellCells(cellID,j) ];
              indx++;
           }
        }
    }
    //RECIEVING allocation (filling will be done by MPI Communication)
    const StorageSite::GatherMap& gatherMap = site.getGatherMap();
    foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
       const StorageSite&  oSite = *mpos.first;
       EntryIndex e(&oSite,&site);
       const Array<int>& recvCounts   =  dynamic_cast< const Array<int>& > (*_recvCounts[e]);
       int recvSize = 0;
       for ( int i = 0; i < recvCounts.getLength(); i++ ){
           recvSize += recvCounts[i];
       }
       //allocate array
       _recvIndices[e] = shared_ptr< Array<int> > ( new Array<int>     (recvSize) ); 
    }
#endif
} 

//fill scatterArray (both mesh and partition) and only gatherArray for mesh
void
Mesh::recvScatterGatherIndicesBufferLocal()
{
#ifdef FVM_PARALLEL
    //RECIEVING allocation (filling will be done by MPI Communication)
    const StorageSite& site     = this->getCells();
    const StorageSite::GatherMap& gatherMap = site.getGatherMap();
    foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
       const StorageSite&  oSite = *mpos.first;
       EntryIndex e(&oSite,&site);
       //mesh interface can be done know
       if ( oSite.getGatherProcID() == - 1) {
          const Mesh& otherMesh = oSite.getMesh();
          *_recvIndices[e] = otherMesh.getSendIndices(e);
       } 
    }
#endif    
}

void
Mesh::syncIndices()
{
#ifdef FVM_PARALLEL
    //SENDING
    const int  request_size = get_request_size();
    MPI::Request   request_send[ request_size ];
    MPI::Request   request_recv[ request_size ];
    int indxSend = 0;
    int indxRecv = 0;
    const StorageSite&    site      = this->getCells();
    const StorageSite::ScatterMap& scatterMap = site.getScatterMap();
    foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
        const StorageSite&  oSite = *mpos.first;
        EntryIndex e(&site,&oSite);
        //checking if storage site is only site or ghost site, we only communicate ghost site ( oSite.getCount() == -1 ) 
        ArrayBase& sendArray = *_sendIndices[e];
        //loop over surround indices and itself
        int to_where  = oSite.getGatherProcID();
        if ( to_where != -1 ){
           int mpi_tag = oSite.getTag();
           request_send[indxSend++] =  
                MPI::COMM_WORLD.Isend( sendArray.getData(), sendArray.getDataSize(), MPI::BYTE, to_where, mpi_tag );
        }
    }
    //RECIEVING
    //getting values from other meshes to fill g
    const StorageSite::GatherMap& gatherMap = site.getGatherMap();
    foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
        const StorageSite&  oSite = *mpos.first;
        EntryIndex e(&oSite,&site);
        ArrayBase& recvArray = *_recvIndices[e];
        int from_where       = oSite.getGatherProcID();
        if ( from_where != -1 ){
           int mpi_tag = oSite.getTag();
           request_recv[indxRecv++] =  
                  MPI::COMM_WORLD.Irecv( recvArray.getData(), recvArray.getDataSize(), MPI::BYTE, from_where, mpi_tag );
        }
     }

     int count  = get_request_size();
     MPI::Request::Waitall( count, request_recv );
     MPI::Request::Waitall( count, request_send );
#endif     

}

////////////////////////PRIVATE METHODS////////////////////////////
void 
Mesh::createRowColSiteCRConn()
{ 
    //counting interface counts
    //counting interfaces
    set<int> interfaceCells;
    //const Array<int>& localToGlobal = this->getLocalToGlobal();
    const map<int,int>& globalToLocal = this->getGlobalToLocal();
    const StorageSite& site = this->getCells();
    const int originalCount = site.getCount();
    const StorageSite::GatherMap& gatherMap = site.getGatherMap();
    int countLevel0 = 0;
    foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
       const StorageSite&  oSite = *mpos.first;
       const Array<int>& gatherArray = dynamic_cast< const Array<int>& > (*mpos.second);
       countLevel0 += gatherArray.getLength();
       EntryIndex e(&oSite,&site);
       const Array<int>  & recv_indices = dynamic_cast< const Array<int>& > (*_recvIndices[e]);
       const Array<int>  & recv_counts  = dynamic_cast< const Array<int>& > (*_recvCounts [e]);
       //loop over gatherArray
       int indx = 0;
       for ( int i = 0; i < gatherArray.getLength(); i++ ){
          const int nnb = recv_counts[i]; //give getCount() 
          for ( int nb = 0; nb < nnb; nb++ ){
              const int localID = globalToLocal.find( recv_indices[indx] )->second;
              if ( localID >= originalCount ){
                 interfaceCells.insert( recv_indices[indx] );
              }
              indx++;
           }
        }
     }
     const int selfCount   = site.getSelfCount();
     const int countLevel1 = int(interfaceCells.size());
     //ghost cells = sum of boundary and interfaces
     const int nghost = getNumBounCells() +  countLevel0 + countLevel1;
     _cellSiteGhostExt  = shared_ptr<StorageSite> ( new StorageSite(selfCount, nghost) );
     //constructing new CRConnecitvity;
     _cellCellsGhostExt = shared_ptr< CRConnectivity> ( new CRConnectivity( *_cellSiteGhostExt, *_cellSiteGhostExt) );
}
 

void
Mesh::countCRConn()
{
    CRConnectivity& conn = *_cellCellsGhostExt;
    conn.initCount();
    const StorageSite& site = this->getCells();
    const CRConnectivity& cellCells = this->getCellCells();
    int ncount = site.getSelfCount() + getNumBounCells();
    //loop over old connectivity (inner + boundary) 
    for ( int i = 0; i < ncount; i++ ){
       conn.addCount(i, cellCells.getCount(i) );
    }
    // now interfaces
    const StorageSite::GatherMap& gatherMap = site.getGatherMap();
    foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
       const StorageSite&  oSite = *mpos.first;
       const Array<int>& gatherArray = dynamic_cast< const Array<int>& > (*mpos.second);
       EntryIndex e(&oSite,&site);
       const Array<int>& recv_counts = dynamic_cast< const Array<int>& > (*_recvCounts [e]);
       //loop over gatherArray
       for ( int i = 0; i < gatherArray.getLength(); i++ ){
          conn.addCount(gatherArray[i], recv_counts[i] );
       }
    }
    //finishCount
    conn.finishCount();
}

void 
Mesh::addCRConn()
{
    CRConnectivity& conn = *_cellCellsGhostExt;
    const StorageSite& site = this->getCells();
    const CRConnectivity& cellCells =this->getCellCells();
    int ncount = site.getSelfCount() + getNumBounCells();
    //first inner
    //loop over olde connectivity (inner + boundary) 
    for ( int i = 0; i < ncount; i++ ){
       for ( int j = 0; j < cellCells.getCount(i); j++ ){
          conn.add(i, cellCells(i,j));
        }
    }
    
    //CRConnectivityPrint( cellCells, 0, "cellCellsBeforeGhostExt");
    
    // now interfaces
    const map<int,int>& globalToLocal = this->getGlobalToLocal();
    const StorageSite::GatherMap& gatherMap = site.getGatherMap();
    foreach(const StorageSite::GatherMap::value_type& mpos, gatherMap){
       const StorageSite&  oSite = *mpos.first;
       const Array<int>& gatherArray = dynamic_cast< const Array<int>& > (*mpos.second);
       EntryIndex e(&oSite,&site);
       const Array<int>& recv_counts  = dynamic_cast< const Array<int>& > (*_recvCounts [e]);
       const Array<int>& recv_indices = dynamic_cast< const Array<int>& > (*_recvIndices[e]);
       //loop over gatherArray
       int indx = 0;
       for ( int i = 0; i < gatherArray.getLength(); i++ ){
          const int ncount = recv_counts[i];
          for ( int j = 0; j < ncount; j++ ){
             const int addCell = globalToLocal.find( recv_indices[indx] )->second;
             conn.add(gatherArray[i], addCell);
             indx++;
          }
       }
    }
    //finish add
    conn.finishAdd();
}

int 
Mesh::getNumBounCells()
{
    //boundary information has been stored
    const FaceGroupList&  boundaryFaceGroups = this->getBoundaryFaceGroups();
    int nBounElm = 0;
    for ( int bounID = 0; bounID < int(boundaryFaceGroups.size()); bounID++){
        nBounElm += boundaryFaceGroups.at(bounID)->site.getCount();
    }
    return nBounElm; 
}


int
Mesh::get_request_size()
{
    int indx =  0;
    const StorageSite& site = this->getCells();
    const StorageSite::ScatterMap& scatterMap = site.getScatterMap();
    foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
        const StorageSite&  oSite = *mpos.first;
        //checking if storage site is only site or ghost site, we only communicate ghost site ( oSite.getCount() == -1 ) 
        if ( oSite.getGatherProcID() != -1 )
           indx++;
    }
    return indx;
}
 
 
void
Mesh::CRConnectivityPrint( const CRConnectivity& conn, int procID, const string& name )
{
#ifdef FVM_PARALLEL
    if ( MPI::COMM_WORLD.Get_rank() == procID ){
        cout <<  name << " :" << endl;
        const Array<int>& row = conn.getRow();
        const Array<int>& col = conn.getCol();
        for ( int i = 0; i < row.getLength()-1; i++ ){
           cout << " i = " << i << ",    ";
           for ( int j = row[i]; j < row[i+1]; j++ )
              cout << col[j] << "  ";
           cout << endl;
        }
    }
#endif    
}

