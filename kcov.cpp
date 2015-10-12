#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <utility>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Frontend/Utils.h"

using namespace clang;
using namespace std;

class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor>
{
public:
    MyASTVisitor(SourceManager& _srcmgr, Rewriter& _rewriter)
      :srcmgr(_srcmgr), rewriter(_rewriter), id(0), branches(0)
    {}

    ~MyASTVisitor() {
      cout << "Total number of branches: " << (this->branches) << endl;
    }

    bool VisitStmt(Stmt *s) {
        SourceLocation startLocation = s->getLocStart();
        unsigned int lineNum = srcmgr.getExpansionLineNumber(startLocation);
        unsigned int colNum = srcmgr.getExpansionColumnNumber(startLocation);
        bool hasDefault = false;
        SwitchCase *child;
        Expr* condition;
        
        if (!srcmgr.getFilename(startLocation).empty()) {
          fileName = srcmgr.getFilename(startLocation);
        }

        if (isa<IfStmt>(s))  {
          printInfo("If", lineNum, colNum, fileName);
          this->branches += 2;
          IfStmt *ifStmt = cast<IfStmt>(s);
          condition = ifStmt->getCond();
          rewriter.InsertTextAfter(condition->getLocStart(), "/* If */");
        }

        else if (isa<SwitchStmt>(s)) {
          child = ((SwitchStmt*) s)->getSwitchCaseList();
          while (child != NULL) {
            if (isa<DefaultStmt>(child)) {
              hasDefault = true;
              break;
            }
            child = child->getNextSwitchCase();
          }

          if (!hasDefault)  {
            printInfo("ImpDef", lineNum, colNum, fileName);
            this->branches += 1;
            SwitchStmt *switchStmt = cast<SwitchStmt>(s);
            condition = switchStmt->getCond();
            rewriter.InsertTextAfter(condition->getLocStart(), "/* ImpDef */");
          }
        }

        else if (isa<ForStmt>(s)) {
          printInfo("For", lineNum, colNum, fileName);
          this->branches += 2;
          ForStmt *forStmt = cast<ForStmt>(s);
          condition = forStmt->getCond();
          rewriter.InsertTextAfter(condition->getLocStart(), "/* For */");
        }  
        
        else if (isa<CaseStmt>(s))  {
          printInfo("Case", lineNum, colNum, fileName);
          this->branches += 1;
          CaseStmt *caseStmt = cast<CaseStmt>(s);
          Expr* LHS = caseStmt->getLHS();
          Expr* RHS = caseStmt->getRHS();

          cout << rewriter.ConvertToString(LHS) << endl;
          cout << rewriter.ConvertToString(RHS) << endl;
          rewriter.InsertTextAfter(caseStmt->getCaseLoc(), "/* Case */");
        }

        else if (isa<ConditionalOperator>(s))  {
          printInfo("?:", lineNum, colNum, fileName);
          this->branches += 2;
          ConditionalOperator *cdOp = cast<ConditionalOperator>(s);
          condition = cdOp->getCond();
          rewriter.InsertTextAfter(condition->getLocStart(), "/* ?: */");
        }

        else if (isa<DoStmt>(s)) {
          printInfo("Do", lineNum, colNum, fileName);
          this->branches += 2;
          DoStmt *doStmt = cast<DoStmt>(s);
          condition = doStmt->getCond();
          rewriter.InsertTextAfter(condition->getLocStart(), "/* Do */");
        } 
        
        else if (isa<WhileStmt>(s))  {
          printInfo("While", lineNum, colNum, fileName);
          this->branches += 2;
          WhileStmt *whileStmt = cast<WhileStmt>(s);
          condition = whileStmt->getCond();
          rewriter.InsertTextAfter(condition->getLocStart(), "/* While */");
        } 

        else if (isa<DefaultStmt>(s))  {
          printInfo("Default", lineNum, colNum, fileName);
          this->branches += 1;
          DefaultStmt *defaultStmt = cast<DefaultStmt>(s);
          rewriter.InsertTextAfter(defaultStmt->getColonLoc(), "/* Default */");
        }
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl *f) {
        // Fill out this function for your homework
        if (f->hasBody()) {
          string funcname = f->getName();
          cout << "function: " << funcname << endl;
        }

        return true;
    }
private:
    string fileName;
    SourceManager &srcmgr;
    Rewriter &rewriter;
    unsigned int id;
    unsigned int branches;

    void printInfo(string type, unsigned int _lineNum, unsigned int _colNum, string _fileName) {
      cout << '\t' << type << "\t" << "ID: " << (this->id) << '\t' << "Line: " << _lineNum << '\t' << "Column: " << _colNum << '\t' << "Filename: " << _fileName << "\t" << endl; 
      id++;
    }
};

class MyASTConsumer : public ASTConsumer
{
public:
    MyASTConsumer(SourceManager& srcmgr, Rewriter& rewriter)
        : Visitor(srcmgr, rewriter) //initialize MyASTVisitor
    {}

    virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
        for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
            // Travel each function declaration using MyASTVisitor
            Visitor.TraverseDecl(*b);
        }
        return true;
    }

private:
    MyASTVisitor Visitor;
};


int main(int argc, char *argv[])
{
    if (argc != 2) {
        llvm::errs() << "Usage: kcov-branch-identify <filename>\n";
        return 1;
    }
    string filename = (string) argv[1];

    // CompilerInstance will hold the instance of the Clang compiler for us,
    // managing the various objects needed to run the compiler.
    CompilerInstance TheCompInst;
    
    // Diagnostics manage problems and issues in compile 
    TheCompInst.createDiagnostics(NULL, false);

    // Set target platform options 
    // Initialize target info with the default triple for our platform.
    TargetOptions *TO = new TargetOptions();
    TO->Triple = llvm::sys::getDefaultTargetTriple();
    TargetInfo *TI = TargetInfo::CreateTargetInfo(TheCompInst.getDiagnostics(), TO);
    TheCompInst.setTarget(TI);

    // FileManager supports for file system lookup, file system caching, and directory search management.
    TheCompInst.createFileManager();
    FileManager &FileMgr = TheCompInst.getFileManager();
    
    // SourceManager handles loading and caching of source files into memory.
    TheCompInst.createSourceManager(FileMgr);
    SourceManager &SourceMgr = TheCompInst.getSourceManager();

    // Prreprocessor runs within a single source file
    TheCompInst.createPreprocessor();
    
    // ASTContext holds long-lived AST nodes (such as types and decls) .
    TheCompInst.createASTContext();

    // Enable HeaderSearch option
    llvm::IntrusiveRefCntPtr<clang::HeaderSearchOptions> hso( new HeaderSearchOptions());
    HeaderSearch headerSearch(hso,
                              TheCompInst.getFileManager(),
                              TheCompInst.getDiagnostics(),
                              TheCompInst.getLangOpts(),
                              TI);

    // <Warning!!> -- Platform Specific Code lives here
    // This depends on A) that you're running linux and
    // B) that you have the same GCC LIBs installed that I do. 
    /*
    $ gcc -xc -E -v -
    ..
     /usr/local/include
     /usr/lib/gcc/x86_64-linux-gnu/4.4.5/include
     /usr/lib/gcc/x86_64-linux-gnu/4.4.5/include-fixed
     /usr/include
    End of search list.
    */
    const char *include_paths[] = {"/usr/local/include",
                "/usr/lib/gcc/x86_64-linux-gnu/4.4.5/include",
                "/usr/lib/gcc/x86_64-linux-gnu/4.4.5/include-fixed",
                "/usr/include"};

    for (int i=0; i<4; i++) 
        hso->AddPath(include_paths[i], 
                    clang::frontend::Angled, 
                    false, 
                    false);
    // </Warning!!> -- End of Platform Specific Code

    InitializePreprocessor(TheCompInst.getPreprocessor(), 
                  TheCompInst.getPreprocessorOpts(),
                  *hso,
                  TheCompInst.getFrontendOpts());


    // A Rewriter helps us manage the code rewriting task.
    Rewriter TheRewriter;
    TheRewriter.setSourceMgr(SourceMgr, TheCompInst.getLangOpts());

    // Set the main file handled by the source manager to the input file.
    const FileEntry *FileIn = FileMgr.getFile(argv[1]);
    SourceMgr.createMainFileID(FileIn);
    
    // Inform Diagnostics that processing of a source file is beginning. 
    TheCompInst.getDiagnosticClient().BeginSourceFile(TheCompInst.getLangOpts(),&TheCompInst.getPreprocessor());
    

    // Create an AST consumer instance which is going to get called by ParseAST.
    Rewriter& ReWr = TheRewriter;
    MyASTConsumer TheConsumer(SourceMgr, ReWr);

    // Parse the file to AST, registering our consumer as the AST consumer.
    ParseAST(TheCompInst.getPreprocessor(), &TheConsumer, TheCompInst.getASTContext());

    const RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(SourceMgr.getMainFileID());
    if (RewriteBuf != NULL)  {
      filename = (filename.substr(0,filename.size()-2)) + "-kcov.c";
      ofstream output(filename.c_str());
      output << string(RewriteBuf->begin(), RewriteBuf->end());
      output.close();
    }

    return 0;
}
